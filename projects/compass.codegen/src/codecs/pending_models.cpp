/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "compass.codegen/codecs/pending_models.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codegen/ops/template_context.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::codegen::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::array<std::uint8_t, 16> pending_namespace{0x6D, 0x1F, 0x3A,
    0x52, 0x8C, 0x7B, 0x4E, 0x0A, 0x9B, 0x64, 0x2F, 0x5D, 0x8A, 0x1C, 0x7E,
    0x90};

constexpr std::string_view derived_on = "2026-10-10";

constexpr std::string_view line_columns = R"(** id
:PROPERTIES:
:type:        uuid
:cpp_type:    boost::uuids::uuid
:primary_key: true
:END:

UUID identifying this line.

One proposed write inside a request.

** request_id
:PROPERTIES:
:type:        uuid
:cpp_type:    boost::uuids::uuid
:natural_key: true
:END:

The approval request this line belongs to.

References the approval requests table.

#+begin_src cpp :name generator
ctx.generate_uuid()
#+end_src

** line_no
:PROPERTIES:
:type:        integer
:cpp_type:    int
:natural_key: true
:END:

The place of this line in its request, from one.

#+begin_src cpp :name generator
1
#+end_src

** operation
:PROPERTIES:
:type:     text
:cpp_type: std::string
:END:

What the line proposes: put or delete.

#+begin_src cpp :name generator
std::string("put")
#+end_src

** base_version
:PROPERTIES:
:type:     integer
:cpp_type: int
:END:

The version of the live row the maker read, or zero for a new row.

#+begin_src cpp :name generator
0
#+end_src

** applied
:PROPERTIES:
:type:          boolean
:cpp_type:      bool
:nullable:      false
:default_value: false
:END:

Whether the apply has written this line. A line is applied once.

#+begin_src cpp :name generator
false
#+end_src

)";

constexpr std::string_view foreign_keys_head = R"(* Foreign keys

** request_id
:PROPERTIES:
:table:         ores_inbox_approval_requests_tbl
:target_column: id
:nullable:      false
:list_by:       true
:error_message: Invalid request_id: %. No approval request found with this id.
:END:

The request the line belongs to.

The seed asks as a service account. A person's account written into the shared
test tenant would end the tenant's bootstrap, and every later test that writes as
the unit test would be refused its modified_by.

#+begin_src cpp :name parent_seed_includes
#include "ores.iam.api/generators/account_generator.hpp"
#include "ores.iam.core/repository/account_repository.hpp"
#include "ores.inbox.api/generators/approval_request_generator.hpp"
#include "ores.inbox.core/repository/approval_kind_repository.hpp"
#include "ores.inbox.core/repository/approval_request_repository.hpp"
#include "ores.inbox.core/repository/approval_request_state_repository.hpp"
#include "ores.utility/uuid/tenant_id.hpp"
#+end_src

#+begin_src cpp :name parent_seed
    {
        auto asker = ores::iam::generators::generate_synthetic_account(ctx);
        asker.account_type = "service";
        asker.change_reason_code = "system.test";
        ores::iam::repository::account_repository asker_repo;
        asker_repo.write(party_ctx, asker);

        auto request = ores::inbox::generators::generate_synthetic_approval_request(ctx);
        request.change_reason_code = "system.test";
        request.requested_by = asker.id;

        const auto system_ctx =
            party_ctx.with_tenant(ores::utility::uuid::tenant_id::system(), h.db_user());
        const auto kinds = ores::inbox::repository::approval_kind_repository{}.read_latest(system_ctx);
        REQUIRE_FALSE(kinds.empty());
        request.kind_code = kinds.front().code;
        const auto states =
            ores::inbox::repository::approval_request_state_repository{}.read_latest(system_ctx);
        REQUIRE_FALSE(states.empty());
        request.state_code = states.front().code;

        ores::inbox::repository::approval_request_repository request_repo;
        request_repo.write(party_ctx, request);
        v.request_id = request.id;
    }
#+end_src

)";

/**
 * @brief A model the script refuses, which stops it.
 */
struct refused : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/**
 * @brief @p text cut before every line that starts with @p marker, as
 * re.split(r"(?m)^(?=marker)") cuts it: the part before the first such
 * line, then one part per line.
 */
std::vector<std::string_view> split_before(std::string_view text,
    std::string_view marker) {
    std::vector<std::string_view> parts;
    std::size_t from = 0;
    for (std::size_t i = 0; i <= text.size();) {
        const bool at_line = i == 0 || text[i - 1] == '\n';
        if (at_line && text.substr(i).starts_with(marker)) {
            if (i > 0 || parts.empty())
                parts.push_back(text.substr(from, i - from));
            from = i;
        }
        const auto nl = text.find('\n', i);
        if (nl == std::string_view::npos)
            break;
        i = nl + 1;
    }
    parts.push_back(text.substr(from));
    return parts;
}

std::string_view first_line(std::string_view text) {
    const auto lines = co::split_lines(text);
    return lines.empty() ? std::string_view{} : lines.front();
}

std::string section_name(std::string_view section) {
    return std::string(co::strip(first_line(section).substr(2)));
}

std::string keyword(std::string_view front, std::string_view name) {
    const pt::regex re("^#\\+" + std::string(name) + R"(:\s*(.*)$)",
        {.multiline = true});
    const auto m = re.search(front);
    return m ? std::string(co::strip(m->group(1))) : std::string();
}

std::string inherited_drawer(std::string_view front) {
    static const pt::regex re(R"(^:ores\.cpp\.wire-codec\.enabled:\s*true\s*$)",
        {.multiline = true});
    return re.search(front) ? ":ores.cpp.wire-codec.enabled: true\n" : "";
}

struct model {
    std::string_view front;
    std::vector<std::string_view> sections;
    std::map<std::string, std::string_view> by_name;
};

model split_model(std::string_view text) {
    auto parts = split_before(text, "* ");
    model m{parts.front(), {parts.begin() + 1, parts.end()}, {}};
    for (const auto s : m.sections)
        m.by_name[section_name(s)] = s;
    return m;
}

bool is_gated(std::string_view text) {
    static const pt::regex flag(R"(^:(gated|pending_change):\s*true\s*$)",
        {.multiline = true});
    for (const auto s : split_model(text).sections)
        if (section_name(s) == "Flags")
            return flag.search(s).has_value();
    return false;
}

std::string remove_lines(const std::string& text, std::string_view pattern) {
    const pt::regex re(pattern, {.multiline = true});
    return re.substitute(text, "");
}

std::string derive_column(std::string chunk) {
    const auto name = std::string(co::strip(first_line(chunk).substr(3)));
    chunk = remove_lines(chunk, R"(^:natural_key:.*\n)");
    static const pt::regex uuid_gen(R"(^uuid_gen\(\)$)", {.multiline = true});
    chunk = uuid_gen.substitute(chunk, "ctx.generate_uuid()");
    if (chunk.find("uuid_gen") != std::string::npos)
        throw refused("the column " + name + " names the generator helper "
            "uuid_gen in a form the derivation does not rewrite");
    if (name == "id") {
        chunk = remove_lines(chunk, R"(^:primary_key:.*\n)");
        if (!chunk.starts_with("** id\n"))
            throw refused("id column heading changed");
        chunk.replace(0, 6, "** entity_id\n");
    }
    return chunk;
}

/**
 * @brief The tables of the gated models in @p dir, by their :tablename:
 * or their component and plural.
 */
std::set<std::string, std::less<>> pending_tables(const ct::corpus_view& v,
    std::string_view dir) {
    static const pt::regex tablename(R"(^:tablename:\s*(\S+))",
        {.multiline = true});
    std::set<std::string, std::less<>> tables;
    const auto prefix = std::string(dir) + "/ores.";
    for (const auto path : v.list_files(prefix, ".org")) {
        if (path.find('/', prefix.size()) != std::string_view::npos)
            continue;
        const auto text = v.read_file(path).value_or("");
        if (!is_gated(text))
            continue;
        if (const auto m = tablename.search(text)) {
            tables.emplace(m->group(1));
            continue;
        }
        const auto front = split_model(text).front;
        auto plural = keyword(front, "entity_plural");
        if (plural.empty())
            plural = keyword(front, "name");
        tables.insert("ores_" + keyword(front, "component") + "_" + plural +
            "_tbl");
    }
    return tables;
}

std::string drop_pending_targets(std::string_view body,
    const std::set<std::string, std::less<>>& tables) {
    static const pt::regex table(R"(^:table:\s*(\S+))", {.multiline = true});
    std::string kept;
    for (const auto entry : split_before(body, "** ")) {
        const auto target = table.search(entry);
        if (target && tables.contains(target->group(1)))
            continue;
        kept += entry;
    }
    return kept;
}

/**
 * @brief The validation table with each row cut to its column and
 * function, as lookups_only leaves it.
 */
std::string lookups_only(std::string_view validations) {
    std::string out;
    std::optional<std::vector<std::size_t>> widths;
    std::size_t from = 0;
    bool first = true;
    while (true) {
        const auto nl = validations.find('\n', from);
        const auto line = validations.substr(from,
            nl == std::string_view::npos ? std::string_view::npos : nl - from);
        if (!first)
            out += '\n';
        first = false;
        auto emit = [&] {
            if (!line.starts_with('|')) {
                out += line;
                return;
            }
            const auto stripped = co::strip(line);
            const bool separator = std::ranges::all_of(stripped,
                [](char c) { return c == '|' || c == '-' || c == '+'; });
            auto inner = stripped;
            while (inner.starts_with('|'))
                inner.remove_prefix(1);
            while (inner.ends_with('|'))
                inner.remove_suffix(1);
            const char cut = separator ? '+' : '|';
            std::vector<std::string_view> cells;
            for (std::size_t i = 0;;) {
                const auto at = inner.find(cut, i);
                cells.push_back(inner.substr(i, at == std::string_view::npos
                    ? std::string_view::npos : at - i));
                if (at == std::string_view::npos)
                    break;
                i = at + 1;
            }
            if (cells.size() <= 2) {
                out += line;
                return;
            }
            if (separator) {
                if (!widths)
                    throw refused("a validation table starts with a rule");
                out += "|";
                for (std::size_t i = 0; i < widths->size(); ++i)
                    (out += i > 0 ? "+" : "") += std::string((*widths)[i],
                        '-');
                out += "|";
                return;
            }
            const auto a = co::strip(cells[0]);
            const auto b = co::strip(cells[1]);
            if (!widths)
                widths = std::vector<std::size_t>{co::characters(a) + 2,
                    co::characters(b) + 2};
            out += "| " + std::string(a) + " | " + std::string(b) + " |";
        };
        emit();
        if (nl == std::string_view::npos)
            break;
        from = nl + 1;
    }
    return out;
}

std::string header(std::string_view front, std::string_view id_name,
    std::string_view component, std::string_view singular,
    std::string_view title) {
    const std::string c(component);
    const std::string s(singular);
    return ":PROPERTIES:\n" + inherited_drawer(front) + ":ID: " +
        ops::uuid5(pending_namespace, id_name) + "\n:END:\n#+title: ores." +
        c + "." + s + "_change\n#+description: GENERATED from ores." + c +
        "." + s + " by derive_pending_models.py. One proposed write to the " +
        s + ", held until its request is decided.\n"
        "#+type: ores.codegen.entity\n#+component: " + c +
        "\n#+filetags: :model:entity:" + c + ":pending:\n"
        "#+brief: One proposed write to a " + s +
        ", held for its request.\n#+entity_singular: " + s +
        "_change\n#+entity_plural: " + s + "_changes\n#+entity_title: " +
        std::string(title) + " Change\n#+created: " +
        std::string(derived_on) + "\n#+updated: " + std::string(derived_on) +
        "\n\nDo not edit. Change ores." + c + "." + s +
        " and run\nderive_pending_models.py.\n\n";
}

std::string sql_and_keys(std::string_view component, std::string_view singular) {
    return "* SQL\n\n** Flags\n:PROPERTIES:\n:tablename: ores_" +
        std::string(component) + "_" + std::string(singular) +
        "_changes_tbl\n:END:\n\n" + std::string(foreign_keys_head);
}

std::string repository(std::string_view singular, std::string_view words) {
    const std::string s(singular);
    const std::string w(words);
    return "* C++\n\n** Flags\n:PROPERTIES:\n:subcomponent:    api\n:END:\n\n"
        "** Repository\n:PROPERTIES:\n:entity_singular_short: " + s +
        "_change\n:entity_plural_short:   " + s +
        "_changes\n:entity_singular_words: " + w +
        " change\n:entity_plural_words:   " + w + " changes\n:END:\n\n";
}

std::string after_first_newline(std::string_view section) {
    const auto nl = section.find('\n');
    if (nl == std::string_view::npos)
        throw refused("a section has no body");
    return std::string(section.substr(nl + 1));
}

std::string_view rstrip_newlines(std::string_view text) {
    while (text.ends_with('\n'))
        text.remove_suffix(1);
    return text;
}

std::string_view strip_newlines(std::string_view text) {
    while (text.starts_with('\n'))
        text.remove_prefix(1);
    while (text.ends_with('\n'))
        text.remove_suffix(1);
    return text;
}

std::string derive_junction(std::string_view file_name, const model& m) {
    const auto component = keyword(m.front, "component");
    const auto singular = keyword(m.front, "name_singular");
    const auto title = keyword(m.front, "name_title");
    for (const auto needed : {"Left", "Right"})
        if (!m.by_name.contains(needed))
            throw refused("a junction with a pending change has no '* " +
                std::string(needed) + "' section");
    auto half = [&](std::string_view section) {
        static const pt::regex prop(R"(^:(\w+):[ \t]+(.*)$)",
            {.multiline = true});
        std::map<std::string, std::string, std::less<>> props;
        for (const auto& p : prop.find_all(section))
            props[std::string(p.group(1))] = p.group(2);
        for (const auto key : {"column", "type", "cpp_type"})
            if (!props.contains(key))
                throw refused("a half of the junction has no :" +
                    std::string(key) + ":");
        if (props["type"] != "uuid")
            throw refused("a half of the junction is not a uuid");
        const auto end = section.find(":END:");
        if (end == std::string_view::npos)
            throw refused("a half of the junction has no drawer end");
        const auto body = strip_newlines(section.substr(end + 5));
        const auto description = co::strip(body.substr(0, body.find("\n\n")));
        return "** " + props["column"] + "\n:PROPERTIES:\n:type:        " +
            props["type"] + "\n:cpp_type:    " + props["cpp_type"] +
            "\n:END:\n\n" + std::string(description) +
            "\n\n#+begin_src cpp :name generator\nctx.generate_uuid()\n"
            "#+end_src\n\n";
    };
    std::string out = header(m.front, file_name, component, singular, title);
    out += "* Flags\n:PROPERTIES:\n:schema:               public\n"
        ":product:              ores\n:component:             " + component +
        "\n:profile:   uuid-identified-lookup\n:client_read_only: true\n"
        ":END:\n\n";
    out += "* Columns\n\n" + std::string(line_columns);
    out += half(m.by_name.at("Left"));
    out += half(m.by_name.at("Right"));
    out += sql_and_keys(component, singular);
    auto words = singular;
    std::ranges::replace(words, '_', ' ');
    out += repository(singular, words);
    out += "** Domain includes\n\n#+begin_src cpp :name includes\n"
        "#include <string>\n#include <boost/uuid/uuid.hpp>\n#+end_src\n\n";
    return out;
}

std::string derive(const ct::corpus_view& v, std::string_view path,
    std::string_view text) {
    const auto m = split_model(text);
    const auto file_name = path.substr(path.rfind('/') + 1);
    if (keyword(m.front, "type") == "ores.codegen.junction")
        return derive_junction(file_name, m);
    const auto component = keyword(m.front, "component");
    const auto singular = keyword(m.front, "entity_singular");
    const auto title = keyword(m.front, "entity_title");
    auto id_name = keyword(m.front, "title");
    if (id_name.empty())
        id_name = file_name;
    std::string out = header(m.front, id_name, component, singular, title);

    for (const auto needed : {"Flags", "Columns", "C++"})
        if (!m.by_name.contains(needed))
            throw refused("gated model has no '* " + std::string(needed) +
                "' section");
    auto flags = remove_lines(std::string(m.by_name.at("Flags")),
        R"(^:(gated|pending_change):.*\n)");
    flags = remove_lines(flags, R"(^:(parent_id_column|hierarchy_name_field|)"
        R"(generate_touch_function):.*\n)");
    static const pt::regex profile(R"(^:profile:.*$)", {.multiline = true});
    if (profile.find_all(flags).empty())
        throw refused("gated model has no ':profile:' flag");
    flags = profile.substitute(flags,
        ":profile:   uuid-identified-lookup\n:client_read_only: true");
    out += flags.ends_with('\n') ? flags : flags + "\n";

    const auto columns = after_first_newline(m.by_name.at("Columns"));
    out += "* Columns\n\n" + std::string(line_columns);
    const auto chunks = split_before(columns, "** ");
    for (std::size_t i = 1; i < chunks.size(); ++i)
        out += derive_column(std::string(chunks[i]));
    out += "\n";

    out += sql_and_keys(component, singular);
    if (const auto fks = m.by_name.find("Foreign keys");
        fks != m.by_name.end()) {
        auto body = std::string(strip_newlines(after_first_newline(
            fks->second)));
        body = remove_lines(body,
            R"(^:(list_by|bump_parent_version|list_by_as_of):.*\n)");
        body = drop_pending_targets(body,
            pending_tables(v, co::parent_of(path)));
        if (!co::strip(body).empty())
            out += body + "\n\n";
    }

    if (const auto trigger = m.by_name.find("Insert trigger");
        trigger != m.by_name.end()) {
        constexpr std::string_view marker = "\n** Validations";
        const auto at = trigger->second.find(marker);
        if (at != std::string_view::npos) {
            const auto rest = trigger->second.substr(at + marker.size());
            const auto validations = split_before(rest, "** ").front();
            auto table = lookups_only(validations);
            while (table.ends_with('\n'))
                table.pop_back();
            out += "* Insert trigger\n\n** Validations" + table + "\n\n";
        }
    }

    out += repository(singular, singular);
    const auto cpp = m.by_name.at("C++");
    std::vector<std::string_view> subsections;
    for (std::size_t from = 0;;) {
        const auto at = cpp.find("\n** ", from);
        if (at == std::string_view::npos) {
            if (from > 0)
                subsections.push_back(cpp.substr(from));
            break;
        }
        if (from > 0)
            subsections.push_back(cpp.substr(from, at - from));
        from = at + 4;
    }
    for (const auto name : {"Domain includes", "Entity includes"})
        for (const auto s : subsections)
            if (s.starts_with(name))
                out += "** " + std::string(rstrip_newlines(s)) +
                    "\n\n";
    return out;
}

void derive_all(const ct::corpus_view& v, ct::file_output& out) {
    auto models = v.list_files("projects/", ".org");
    std::erase_if(models, [](std::string_view p) {
        return !co::glob_matches("projects/*/modeling/ores.*.org", p);
    });
    std::ranges::stable_sort(models, co::component_less);
    for (const auto path : models) {
        const auto text = v.read_file(path).value_or("");
        if (!is_gated(text))
            continue;
        const auto front = split_model(text).front;
        auto singular = keyword(front, "entity_singular");
        if (singular.empty())
            singular = keyword(front, "name_singular");
        const auto target = std::string(co::parent_of(path)) + "/ores." +
            keyword(front, "component") + "." + singular + "_change.org";
        try {
            out.files.add(target, derive(v, path, text));
        } catch (const refused& e) {
            co::refuse(out, target, std::string(path) + ": " + e.what(),
                "Correct the gated model the reason names.");
        }
    }
}

}

const ct::descriptor pending_models{
    .name = "pending_models",
    .version = 1,
    .summary = "Writes the pending-change model of every gated model.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = co::refusal_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{derive_all}};

}

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
#include "compass.crosscheck/codecs/service_integrity.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codegen/ops/values.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.model/ops/builder.hpp"
#include "compass.model/ops/load.hpp"
#include "compass.model/ops/outline.hpp"
#include "compass.platform/text/decoding.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace cops = compass::codec::ops;
namespace mo = compass::model::ops;
namespace mt = compass::model::types;
namespace pt = compass::platform::text;
namespace values = compass::codegen::ops::values;

constexpr std::string_view codec_name = "service_integrity";
constexpr std::string_view check_document =
    "compass/checks/service_integrity.org";

constexpr std::uint32_t unregistered = 0;
constexpr std::uint32_t partless = 1;
constexpr std::uint32_t unknown_runtime = 2;
constexpr std::uint32_t unmet_obligation = 3;
constexpr std::uint32_t second_list = 4;
constexpr std::uint32_t unreadable = 5;

constexpr std::array<ct::rule, 6> rules{{
    {"unregistered", "Every service part, a component overview of the "
        "part kind the check document names, has an entry in the service "
        "registry.", {}, ct::blame_method::added},
    {"partless", "Every registry entry whose runtime has parts names a "
        "service part that exists."},
    {"unknown_runtime", "Every registry entry's runtime is one the check "
        "document's Runtimes table lists.", {}, ct::blame_method::line},
    {"unmet_obligation", "Every service meets each obligation of the check "
        "document's Obligations table that applies to it.",
        {}, ct::blame_method::added},
    {"second_list", "No file the check document scans, but the registry, "
        "lists the registry's services as a literal; the registry is their "
        "one source.", {}, ct::blame_method::line},
    {"unreadable", "The check document gives every table, column and "
        "setting the codec reads, and each row reads."}}};

constexpr std::array<std::string_view, 2> setting_columns{"setting",
    "value"};
constexpr std::array<std::string_view, 2> runtime_columns{"runtime",
    "has part"};
constexpr std::array<std::string_view, 6> obligation_columns{"obligation",
    "test", "path", "value", "condition", "reason"};
constexpr std::array<std::string_view, 2> glob_columns{"glob", "reason"};

constexpr std::array<cops::table_need, 5> needed{{
    {"Settings", setting_columns},
    {"Runtimes", runtime_columns},
    {"Obligations", obligation_columns},
    {"Single source files", glob_columns},
    {"Single source exceptions", glob_columns}}};

constexpr std::array<std::string_view, 4> builtin_fields{"name", "runtime",
    "enabled", "part"};

enum class test { exists, contains, has_line, lacks };

constexpr std::array<std::pair<std::string_view, test>, 4> tests{{
    {"exists", test::exists}, {"contains", test::contains},
    {"has_line", test::has_line}, {"lacks", test::lacks}}};

/**
 * @brief One piece of a pattern: literal text, or a field of the service,
 * whole or one of its pieces split at '.'.
 */
struct piece {
    std::string text;
    std::string field;
    std::optional<std::size_t> index;
};

using pattern = std::vector<piece>;

/**
 * @brief One word of a condition: a field that must be set, or, negated,
 * one that must not.
 */
struct term {
    bool negated = false;
    std::string field;
};

struct obligation {
    std::string name;
    test kind = test::exists;
    pattern path;
    pattern value;
    std::vector<term> condition;
    std::string reason;
    std::uint32_t line = 0;
};

/**
 * @brief Everything the check document states.
 */
struct project_data {
    std::string part_kind;
    std::string default_runtime;
    std::size_t minimum = 3;
    std::map<std::string, bool, std::less<>> runtimes;
    std::vector<obligation> obligations;
    std::vector<std::string> scanned;
    std::vector<std::string> excepted;
};

using fields = std::map<std::string, std::string, std::less<>>;

struct service {
    std::string document;
    std::uint32_t offset = 0;
    fields values;

    [[nodiscard]] const std::string& name() const {
        return values.find("name")->second;
    }
};

struct part {
    std::string name;
    std::string document;
    std::uint32_t offset = 0;
    std::string directory;
};

std::string lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return out;
}

std::vector<std::string> words_of(std::string_view text) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < text.size()) {
        const auto begin = text.find_first_not_of(" \t", at);
        if (begin == std::string_view::npos)
            break;
        auto end = text.find_first_of(" \t", begin);
        if (end == std::string_view::npos)
            end = text.size();
        out.emplace_back(text.substr(begin, end - begin));
        at = end;
    }
    return out;
}

/**
 * @brief The pieces of @p text, or why it is not a pattern.
 */
std::expected<pattern, std::string> pattern_of(std::string_view text) {
    pattern out;
    std::string literal;
    std::size_t at = 0;
    while (at < text.size()) {
        if (text[at] == '}')
            return std::unexpected("a '}' closes no field");
        if (text[at] != '{') {
            literal.push_back(text[at++]);
            continue;
        }
        const auto close = text.find('}', at);
        if (close == std::string_view::npos)
            return std::unexpected("a '{' is not closed");
        auto inside = text.substr(at + 1, close - at - 1);
        piece p;
        if (const auto open = inside.find('['); open != std::string_view::npos) {
            if (!inside.ends_with(']'))
                return std::unexpected("the field {" + std::string(inside) +
                    "} has a '[' that ']' does not end");
            const auto digits = inside.substr(open + 1,
                inside.size() - open - 2);
            std::size_t index = 0;
            const auto [end, error] = std::from_chars(digits.data(),
                digits.data() + digits.size(), index);
            if (digits.empty() || error != std::errc{} ||
                end != digits.data() + digits.size())
                return std::unexpected("the field {" + std::string(inside) +
                    "} has an index that is not a number");
            p.index = index;
            inside = inside.substr(0, open);
        }
        if (inside.empty() || inside.find_first_of("{ ") !=
                std::string_view::npos)
            return std::unexpected("the field {" + std::string(inside) +
                "} has no name");
        if (!literal.empty())
            out.push_back({.text = std::move(literal), .field = {},
                .index = std::nullopt});
        literal.clear();
        p.field = std::string(inside);
        out.push_back(std::move(p));
        at = close + 1;
    }
    if (!literal.empty())
        out.push_back({.text = std::move(literal), .field = {},
            .index = std::nullopt});
    return out;
}

/**
 * @brief The value of @p field of @p s, or null when it has none.
 */
const std::string* field_of(const fields& s, std::string_view field) {
    const auto it = s.find(field);
    return it == s.end() || it->second.empty() ? nullptr : &it->second;
}

/**
 * @brief @p p with the fields of @p s, or nothing when @p s lacks one.
 */
std::optional<std::string> expand(const pattern& p, const fields& s) {
    std::string out;
    for (const auto& x : p) {
        if (x.field.empty()) {
            out += x.text;
            continue;
        }
        const auto* value = field_of(s, x.field);
        if (value == nullptr)
            return std::nullopt;
        if (!x.index) {
            out += *value;
            continue;
        }
        std::string_view rest = *value;
        for (std::size_t k = 0; k < *x.index; ++k) {
            const auto dot = rest.find('.');
            if (dot == std::string_view::npos)
                return std::nullopt;
            rest.remove_prefix(dot + 1);
        }
        out += rest.substr(0, rest.find('.'));
    }
    return out;
}

bool holds(const std::vector<term>& condition, const fields& s) {
    return std::ranges::all_of(condition, [&](const term& t) {
        const auto* value = field_of(s, t.field);
        const bool set = value != nullptr && lower(*value) != "false";
        return set != t.negated;
    });
}

/**
 * @brief True when @p glob matches all of @p s: '*' and '?' within a
 * segment, '**' across segments, and "**" followed by '/' also matching no
 * segment.
 */
bool glob_match(std::string_view glob, std::string_view s) {
    if (glob.empty())
        return s.empty();
    if (glob.starts_with("**")) {
        const auto rest = glob.substr(2);
        if (rest.starts_with('/') && glob_match(rest.substr(1), s))
            return true;
        for (std::size_t i = 0; i <= s.size(); ++i)
            if (glob_match(rest, s.substr(i)))
                return true;
        return false;
    }
    if (glob[0] == '*') {
        for (std::size_t i = 0; i <= s.size(); ++i) {
            if (glob_match(glob.substr(1), s.substr(i)))
                return true;
            if (i < s.size() && s[i] == '/')
                break;
        }
        return false;
    }
    if (s.empty())
        return false;
    if (glob[0] == '?')
        return s[0] != '/' && glob_match(glob.substr(1), s.substr(1));
    return glob[0] == s[0] && glob_match(glob.substr(1), s.substr(1));
}

bool matches(std::string_view glob, std::string_view path) {
    if (glob.find('/') != std::string_view::npos)
        return glob_match(glob, path);
    const auto slash = path.rfind('/');
    return glob_match(glob, slash == std::string_view::npos ? path
        : path.substr(slash + 1));
}

/**
 * @brief What the check document states, or why it states it wrongly,
 * written for the reader of the finding.
 */
std::expected<project_data, std::string> read_document(
    const cops::data_tables& data) {
    const auto wrong = [&](const cops::data_table& table, std::uint32_t line,
                           const std::string& what) {
        return std::unexpected(std::format("{}:{}: {}, in the table \"{}\" "
            "{} reads", data.source, line, what, table.name, codec_name));
    };
    project_data out;

    const auto& s = *data.find("Settings");
    std::set<std::string, std::less<>> seen;
    for (std::size_t r = 0; r < s.rows.size(); ++r) {
        const auto& name = s.rows[r][s.column("setting")];
        const auto& value = s.rows[r][s.column("value")];
        if (!seen.insert(name).second)
            return wrong(s, s.row_lines[r], "the setting '" + name +
                "' is stated twice");
        if (value.empty())
            return wrong(s, s.row_lines[r], "the setting '" + name +
                "' has no value");
        if (name == "part_kind")
            out.part_kind = value;
        else if (name == "default_runtime")
            out.default_runtime = value;
        else if (name == "single_source_minimum") {
            std::size_t n = 0;
            const auto [end, error] = std::from_chars(value.data(),
                value.data() + value.size(), n);
            if (error != std::errc{} || end != value.data() + value.size() ||
                n < 2)
                return wrong(s, s.row_lines[r], "the setting "
                    "'single_source_minimum' is '" + value + "', not a "
                    "number of two or more");
            out.minimum = n;
        } else
            return wrong(s, s.row_lines[r], "the setting '" + name +
                "' is not one " + std::string(codec_name) + " has");
    }
    for (const auto key : {"part_kind", "default_runtime",
             "single_source_minimum"})
        if (!seen.contains(key))
            return wrong(s, s.line, "no row states the setting '" +
                std::string(key) + "'");

    const auto& rt = *data.find("Runtimes");
    for (std::size_t r = 0; r < rt.rows.size(); ++r) {
        const auto& name = rt.rows[r][rt.column("runtime")];
        const auto has = lower(rt.rows[r][rt.column("has part")]);
        if (name.empty())
            return wrong(rt, rt.row_lines[r], "the row names no runtime");
        if (has != "yes" && has != "no")
            return wrong(rt, rt.row_lines[r], "the runtime " + name +
                " has part '" + has + "', not yes or no");
        if (!out.runtimes.emplace(name, has == "yes").second)
            return wrong(rt, rt.row_lines[r], "the runtime " + name +
                " is stated twice");
    }
    if (!out.runtimes.contains(out.default_runtime))
        return wrong(rt, rt.line, "the default runtime " +
            out.default_runtime + " is no row");

    const auto& o = *data.find("Obligations");
    std::set<std::string, std::less<>> names;
    for (std::size_t r = 0; r < o.rows.size(); ++r) {
        const auto& row = o.rows[r];
        const auto line = o.row_lines[r];
        obligation ob;
        ob.name = row[o.column("obligation")];
        ob.reason = row[o.column("reason")];
        ob.line = line;
        if (ob.name.empty())
            return wrong(o, line, "the row names no obligation");
        if (!names.insert(ob.name).second)
            return wrong(o, line, "the obligation '" + ob.name +
                "' is stated twice");
        const auto& kind = row[o.column("test")];
        const auto t = std::ranges::find(tests, kind,
            &std::pair<std::string_view, test>::first);
        if (t == tests.end())
            return wrong(o, line, "the obligation '" + ob.name + "' has the "
                "test '" + kind + "', not exists, contains, has_line or "
                "lacks");
        ob.kind = t->second;
        const auto& path = row[o.column("path")];
        const auto& value = row[o.column("value")];
        if (path.empty())
            return wrong(o, line, "the obligation '" + ob.name +
                "' names no path");
        if (ob.kind != test::exists && value.empty())
            return wrong(o, line, "the obligation '" + ob.name + "' tests "
                "a value and names none");
        if (ob.kind == test::exists && !value.empty())
            return wrong(o, line, "the obligation '" + ob.name + "' tests "
                "that a file exists and names a value");
        auto p = pattern_of(path);
        if (!p)
            return wrong(o, line, "the path of '" + ob.name + "': " +
                p.error());
        ob.path = std::move(*p);
        auto v = pattern_of(value);
        if (!v)
            return wrong(o, line, "the value of '" + ob.name + "': " +
                v.error());
        ob.value = std::move(*v);
        for (const auto& word : words_of(row[o.column("condition")])) {
            term c{.negated = word.starts_with('!'), .field = word};
            if (c.negated)
                c.field.erase(0, 1);
            if (c.field.empty())
                return wrong(o, line, "the condition of '" + ob.name +
                    "' has a '!' that names no field");
            ob.condition.push_back(std::move(c));
        }
        out.obligations.push_back(std::move(ob));
    }

    const auto globs = [&](std::string_view name,
                           std::vector<std::string>& to)
        -> std::optional<std::string> {
        const auto& g = *data.find(name);
        for (std::size_t r = 0; r < g.rows.size(); ++r) {
            const auto& glob = g.rows[r][g.column("glob")];
            if (glob.empty())
                return wrong(g, g.row_lines[r], "the row names no glob")
                    .error();
            to.push_back(glob);
        }
        return std::nullopt;
    };
    if (const auto e = globs("Single source files", out.scanned))
        return std::unexpected(*e);
    if (const auto e = globs("Single source exceptions", out.excepted))
        return std::unexpected(*e);
    return out;
}

/**
 * @brief The fields @p ob names that no service of @p services has and
 * that are not built in.
 */
std::vector<std::string> unknown_fields(const obligation& ob,
    const std::vector<service>& services) {
    std::set<std::string> named;
    for (const auto* p : {&ob.path, &ob.value})
        for (const auto& x : *p)
            if (!x.field.empty())
                named.insert(x.field);
    for (const auto& c : ob.condition)
        named.insert(c.field);
    std::vector<std::string> out;
    for (const auto& f : named) {
        if (std::ranges::find(builtin_fields, f) != builtin_fields.end())
            continue;
        if (std::ranges::any_of(services, [&](const service& s) {
                return s.values.contains(f);
            }))
            continue;
        out.push_back(f);
    }
    return out;
}

std::uint32_t offset_in(std::string_view source, std::string_view part) {
    return static_cast<std::uint32_t>(part.data() - source.data());
}

/**
 * @brief The services of the registry @p d, or why it does not load.
 */
std::expected<std::vector<service>, std::string> services_of(
    const ct::corpus_view& v, const ct::document& d,
    const std::string& default_runtime) {
    const mo::document doc{d.path, d.source, &d.tables};
    const mo::document_finder find = [&v](std::string_view path)
        -> std::optional<mo::document> {
        const auto found = v.find_path(path);
        if (!found)
            return std::nullopt;
        return mo::document{found->path, found->source, &found->tables};
    };
    const mo::document_lister list = [&v](std::string_view prefix) {
        return v.list_files(prefix, ".org");
    };
    mt::model_tables tables;
    mo::builder b(tables);
    mo::load_cache cache;
    std::string error;
    const auto id = mo::load_model(b, doc, mt::model_type::service_registry,
        find, list, cache, &error);
    if (!error.empty())
        return std::unexpected(error);

    mo::outline o;
    mo::build_outline(d.source, d.tables, o);
    std::map<std::string_view, mo::node_id, std::less<>> headings;
    for (const auto n : mo::children(o, mo::node_id{0}))
        if (mo::property(o, n, "replicas").found)
            headings.emplace(o.titles[n.value], n);

    std::vector<service> out;
    const auto registry = values::member(tables, tables.models.roots[id],
        "service_registry");
    for (const auto entry : values::items(tables, registry, "services")) {
        const auto name = values::str(tables, entry, "name");
        service s;
        s.document = std::string(d.path);
        if (const auto h = headings.find(name); h != headings.end()) {
            s.offset = offset_in(d.source, o.titles[h->second.value]);
            for (const auto& [key, value] : mo::properties(o, h->second))
                s.values.emplace(std::string(key), std::string(value));
        }
        const auto runtime = values::str(tables, entry, "runtime");
        s.values["name"] = std::string(name);
        s.values["runtime"] = runtime.empty() ? default_runtime
                                              : std::string(runtime);
        s.values["enabled"] = values::is_true(tables, entry, "enabled")
            ? "true" : "false";
        s.values.erase("part");
        out.push_back(std::move(s));
    }
    return out;
}

/**
 * @brief The service part the component overview @p d is, or nothing when
 * it is no overview of @p kind.
 */
std::optional<part> part_of(const ct::document& d, std::string_view kind) {
    if (!d.path.ends_with("component_overview.org"))
        return std::nullopt;
    mo::outline o;
    mo::build_outline(d.source, d.tables, o);
    const auto k = mo::frontmatter(o, "component_kind");
    if (!k.found || co::strip(k.value) != kind)
        return std::nullopt;
    const auto name = mo::frontmatter(o, "full_name");
    part out;
    out.document = std::string(d.path);
    if (name.found) {
        out.name = std::string(co::strip(name.value));
        out.offset = offset_in(d.source, name.value);
    }
    auto directory = co::parent_of(d.path);
    if (directory == "modeling" || directory.ends_with("/modeling"))
        directory = co::parent_of(directory);
    out.directory = std::string(directory);
    return out;
}

bool name_character(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
}

bool separator(char c) {
    return std::string_view(" \t\r\n,'\"`()[]{};\\").find(c) !=
        std::string_view::npos;
}

struct occurrence {
    std::size_t begin = 0;
    std::size_t end = 0;
    const std::string* name = nullptr;
};

/**
 * @brief Each literal list of at least @p minimum distinct @p names in
 * @p text: the occurrences of a run whose gaps hold only separators.
 */
std::vector<std::vector<occurrence>> lists_in(std::string_view text,
    const std::vector<std::string>& names, std::size_t minimum) {
    std::vector<occurrence> found;
    for (const auto& name : names) {
        for (auto at = text.find(name); at != std::string_view::npos;
             at = text.find(name, at + 1)) {
            const auto end = at + name.size();
            if ((at > 0 && name_character(text[at - 1])) ||
                (end < text.size() && name_character(text[end])))
                continue;
            found.push_back({at, end, &name});
        }
    }
    std::ranges::sort(found, {}, &occurrence::begin);
    std::vector<std::vector<occurrence>> out;
    std::vector<occurrence> run;
    const auto close = [&] {
        std::set<std::string_view> distinct;
        for (const auto& o : run)
            distinct.insert(*o.name);
        if (distinct.size() >= minimum)
            out.push_back(run);
        run.clear();
    };
    for (const auto& o : found) {
        if (!run.empty()) {
            const auto gap = text.substr(run.back().end,
                o.begin - run.back().end);
            if (!std::ranges::all_of(gap, separator))
                close();
        }
        run.push_back(o);
    }
    close();
    return out;
}

std::string describe(const obligation& ob, std::string_view path,
    std::string_view value) {
    switch (ob.kind) {
    case test::exists: return std::string(path) + " does not exist";
    case test::contains:
        return std::string(path) + " does not hold '" + std::string(value) +
            "'";
    case test::has_line:
        return std::string(path) + " has no line '" + std::string(value) +
            "'";
    case test::lacks:
        return std::string(path) + " holds '" + std::string(value) + "'";
    }
    return {};
}

std::string fix_of(const obligation& ob, std::string_view path,
    std::string_view value) {
    switch (ob.kind) {
    case test::exists: return "Create " + std::string(path) + ".";
    case test::contains:
    case test::has_line:
        return "Add '" + std::string(value) + "' to " + std::string(path) +
            ", or regenerate it from the registry.";
    case test::lacks:
        return "Replace '" + std::string(value) + "' in " +
            std::string(path) + ".";
    }
    return {};
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    const auto data = cops::tables_for(codec_name, v.option(cops::data_option),
        needed);
    if (!data) {
        co::report(out, check_document, 0, unreadable, data.error(),
            "Give the tables the message names.");
        return;
    }
    const auto document = read_document(*data);
    if (!document) {
        co::report(out, data->source, 0, unreadable, document.error(),
            "Correct the row the message names.");
        return;
    }
    const auto& d = *document;

    std::vector<service> services;
    std::vector<std::string> registries;
    std::map<std::string, part, std::less<>> parts;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto doc = v[i];
        const mo::document m{doc.path, doc.source, &doc.tables};
        const auto type = mo::model_type_of(mo::declared_type(m));
        if (type == mt::model_type::service_registry) {
            auto loaded = services_of(v, doc, d.default_runtime);
            if (!loaded) {
                co::report(out, doc.path, 0, unreadable, std::string(doc.path) +
                    ": the service registry does not load: " + loaded.error(),
                    "Correct the registry.");
                return;
            }
            registries.emplace_back(doc.path);
            for (auto& s : *loaded)
                services.push_back(std::move(s));
        } else if (type == mt::model_type::component) {
            if (auto p = part_of(doc, d.part_kind); p && !p->name.empty())
                parts.emplace(p->name, std::move(*p));
        }
    }
    if (registries.empty()) {
        co::report(out, data->source, 0, unreadable, "no document of type "
            "ores.codegen.service_registry is in the checkout, so " +
            std::string(codec_name) + " has no services to check",
            "Add the service registry.");
        return;
    }
    bool stop = false;
    for (const auto& ob : d.obligations) {
        const auto unknown = unknown_fields(ob, services);
        if (unknown.empty())
            continue;
        co::report(out, data->source, 0, unreadable, std::format("{}:{}: the "
            "obligation '{}' names the field {}, which no registry entry "
            "has, in the table \"Obligations\" {} reads", data->source,
            ob.line, ob.name, unknown.front(), codec_name),
            "Correct the field, or add it to the registry.");
        stop = true;
    }
    if (stop)
        return;

    std::set<std::string, std::less<>> registered;
    for (auto& s : services) {
        registered.insert(s.name());
        const auto& runtime = s.values["runtime"];
        const auto r = d.runtimes.find(runtime);
        if (r == d.runtimes.end())
            co::report(out, s.document, s.offset, unknown_runtime,
                s.name() + " has the runtime " + runtime + ", which the "
                "check document's Runtimes table does not list",
                "Correct the runtime, or add it to " + data->source + ".");
        const auto p = parts.find(s.name());
        if (p != parts.end())
            s.values["part"] = p->second.directory;
        else if (r != d.runtimes.end() && r->second)
            co::report(out, s.document, s.offset, partless, s.name() +
                " runs on " + runtime + ", whose entries are built from a "
                "service part, and no component overview of kind " +
                d.part_kind + " has the full name " + s.name(),
                "Add the service part, or correct the entry's name or "
                "runtime.");
    }
    for (const auto& [name, p] : parts) {
        if (registered.contains(name))
            continue;
        const auto message = name + " is a service part with no entry in "
            "the service registry, so nothing the registry drives "
            "provisions it";
        const auto fix = "Add a heading " + name + " to " +
            registries.front() + ".";
        out.add({.document = compass::org::types::document_id::none(),
            .range = {p.offset, p.offset},
            .rule = ct::rule_id{unregistered},
            .level = ct::severity::error,
            .message = message,
            .fix = fix,
            .file = p.document,
            .subject = p.document});
    }

    std::map<std::string, std::optional<std::string>, std::less<>> texts;
    const auto text_of = [&](const std::string& path)
        -> const std::optional<std::string>& {
        auto it = texts.find(path);
        if (it == texts.end()) {
            auto bytes = v.read_file(path);
            if (bytes)
                bytes = pt::universal_newlines(pt::decode_replacing(*bytes));
            it = texts.emplace(path, std::move(bytes)).first;
        }
        return it->second;
    };
    for (const auto& s : services) {
        for (const auto& ob : d.obligations) {
            if (!holds(ob.condition, s.values))
                continue;
            const auto path = expand(ob.path, s.values);
            const auto value = expand(ob.value, s.values);
            if (!path || !value)
                continue;
            bool met = true;
            if (ob.kind == test::exists)
                met = v.file_exists(*path);
            else {
                const auto& text = text_of(*path);
                if (ob.kind == test::lacks)
                    met = !text || text->find(*value) == std::string::npos;
                else if (!text)
                    met = false;
                else if (ob.kind == test::contains)
                    met = text->find(*value) != std::string::npos;
                else
                    met = std::ranges::any_of(co::split_lines_keeping_ends(
                        *text), [&](std::string_view line) {
                            return co::strip(line) == *value;
                        });
            }
            if (met)
                continue;
            auto message = s.name() + " misses the obligation \"" + ob.name +
                "\": " + describe(ob, *path, *value);
            if (!ob.reason.empty())
                message += "; " + ob.reason;
            const auto fix = fix_of(ob, *path, *value);
            // The requirement arose when the file gained or lost the value,
            // or, for a file that is not there, when the service was added.
            const auto p = parts.find(s.name());
            const bool held = ob.kind != test::exists && text_of(*path);
            const std::string_view subject = held ? std::string_view(*path)
                : p != parts.end() ? std::string_view(p->second.document)
                                   : std::string_view(s.document);
            out.add({.document = compass::org::types::document_id::none(),
                .range = {s.offset, s.offset},
                .rule = ct::rule_id{unmet_obligation},
                .level = ct::severity::error,
                .message = message,
                .fix = fix,
                .file = s.document,
                .subject = subject,
                .subject_text = held ? std::string_view(*value)
                                     : std::string_view{}});
        }
    }

    std::vector<std::string> names;
    for (const auto& s : services)
        names.push_back(s.name());
    std::ranges::sort(names);
    names.erase(std::unique(names.begin(), names.end()), names.end());
    if (names.size() < d.minimum)
        return;
    for (const auto path : v.list_files("")) {
        if (std::ranges::find(registries, path) != registries.end())
            continue;
        const auto glob_of = [&](const std::string& g) {
            return matches(g, path);
        };
        if (!std::ranges::any_of(d.scanned, glob_of) ||
            std::ranges::any_of(d.excepted, glob_of))
            continue;
        const auto bytes = v.read_file(path);
        if (!bytes)
            continue;
        for (const auto& list : lists_in(*bytes, names, d.minimum)) {
            std::string shown;
            for (std::size_t k = 0; k < list.size() && k < 3; ++k)
                shown += (k == 0 ? "" : ", ") + *list[k].name;
            if (list.size() > 3)
                shown += ", ...";
            co::report(out, path, static_cast<std::uint32_t>(
                    list.front().begin), second_list, std::format("{} lists "
                "{} registry services as a literal ({}); the service "
                "registry is their one source, and a second list leaves a "
                "new service out", path, list.size(), shown),
                "Read the services from " + registries.front() + ", or "
                "except the file in " + data->source + ".");
        }
    }
}

constexpr std::array<ct::option, 1> options{{
    {cops::data_option, "", "The tables of the check document whose "
        "Definition names this codec: the settings that say what a service "
        "part is, the runtimes, the obligations every service meets, and "
        "the files that must not list the services."}}};

}

const ct::descriptor service_integrity{
    .name = "service_integrity",
    .version = 1,
    .summary = "Reports each service that is not whole: unregistered, "
               "without its part, or missing an obligation the project's "
               "check document lists, and each second list of the services.",
    .reads = ct::inputs::every_document(),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}

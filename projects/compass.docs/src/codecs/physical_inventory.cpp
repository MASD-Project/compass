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
#include "compass.docs/codecs/physical_inventory.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::string_view templates_dir =
    "projects/ores.codegen/library/templates/";

/**
 * @brief One node of the physical-space graph, as parse_doc reads it.
 */
struct node {
    std::string path;
    std::string id;
    std::map<std::string, std::string, std::less<>> keywords;

    [[nodiscard]] std::string_view keyword(std::string_view key) const {
        const auto it = keywords.find(key);
        return it == keywords.end() ? std::string_view{} : it->second;
    }

    [[nodiscard]] std::string title() const {
        const auto it = keywords.find("title");
        return it == keywords.end() ? std::string(co::stem_of(path))
            : it->second;
    }

    [[nodiscard]] std::string link() const {
        return "[[id:" + id + "][" + title() + "]]";
    }
};

node parse(std::string_view path, std::string_view text) {
    static const pt::regex id(R"(^:ID:\s*(\S+))", {.multiline = true});
    static const pt::regex keyword(R"(^#\+(\w[\w-]*):\s*(.*)$)");
    node n{std::string(path), {}, {}};
    if (const auto m = id.search(text))
        n.id = m->group(1);
    for (const auto line : co::split_lines(text)) {
        if (const auto m = keyword.match(line)) {
            std::string key(m->group(1));
            std::ranges::transform(key, key.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            n.keywords[key] = co::strip(m->group(2));
        } else if (line.starts_with("* ")) {
            break;
        }
    }
    return n;
}

std::string build_table(const std::vector<std::string_view>& headers,
    const std::vector<std::vector<std::string>>& rows) {
    std::string out = "|";
    std::string sep = "|";
    for (std::size_t i = 0; i < headers.size(); ++i) {
        (out += " ") += std::string(headers[i]) + " |";
        (sep += i > 0 ? "+" : "") += std::string(headers[i].size() + 2, '-');
    }
    out += "\n" + sep + "|";
    for (const auto& r : rows) {
        out += "\n|";
        for (const auto& cell : r)
            (out += " ") += cell + " |";
    }
    return out;
}

std::string replace_section_table(const std::string& text,
    std::string_view heading, const std::string& table) {
    const pt::regex section("(^\\* " + pt::escape(heading) +
        "\\n)(.*?)(?=^\\* |\\z)", {.dotall = true, .multiline = true});
    const auto sec = section.search(text);
    if (!sec) {
        static const pt::regex see_also(R"(^\* See also)",
            {.multiline = true});
        const auto block = "* " + std::string(heading) + "\n\n" + table +
            "\n\n";
        if (const auto m = see_also.search(text))
            return text.substr(0, m->start()) + block + text.substr(m->start());
        auto head = std::string_view(text);
        while (head.ends_with('\n'))
            head.remove_suffix(1);
        return std::string(head) + "\n\n" + block;
    }
    static const pt::regex table_re(R"(^\|.*(?:\n\|.*)*)",
        {.multiline = true});
    const std::string body(sec->group(2));
    std::string fresh;
    if (const auto t = table_re.search(body)) {
        fresh = body.substr(0, t->start()) + table + body.substr(t->end());
    } else {
        auto head = std::string_view(body);
        while (head.ends_with('\n'))
            head.remove_suffix(1);
        fresh = std::string(head) + "\n\n" + table + "\n";
    }
    return text.substr(0, sec->start(2)) + fresh + text.substr(sec->end(2));
}

using group = std::map<std::string, std::vector<const node*>, std::less<>>;

std::vector<const node*> sorted_by_title(const group& g,
    std::string_view key) {
    const auto it = g.find(key);
    if (it == g.end())
        return {};
    auto out = it->second;
    std::ranges::stable_sort(out, {}, &node::title);
    return out;
}

std::size_t count_of(const group& g, std::string_view key) {
    const auto it = g.find(key);
    return it == g.end() ? 0 : it->second.size();
}

void inventories(const ct::corpus_view& v, ct::file_output& out) {
    std::vector<node> docs;
    for (const auto path : v.list_files(templates_dir, ".org")) {
        if (path.find('/', templates_dir.size()) == std::string_view::npos)
            docs.push_back(parse(path, v.read_file(path).value_or("")));
    }
    std::vector<const node*> tspaces;
    std::vector<const node*> facets;
    group facets_by_ts;
    group arch_by_facet;
    for (const auto& d : docs) {
        const auto type = d.keyword("type");
        if (type == "technical_space") {
            tspaces.push_back(&d);
        } else if (type == "facet") {
            facets.push_back(&d);
            if (d.title().starts_with("ores."))
                facets_by_ts[std::string(d.keyword("facet_group"))]
                    .push_back(&d);
        } else if (type == "archetype") {
            arch_by_facet[std::string(d.keyword("facet"))].push_back(&d);
        }
    }

    struct edit {
        const node* doc;
        std::string_view heading;
        std::string table;
    };
    std::vector<edit> edits;
    std::vector<const node*> real_ts;
    for (const auto* d : tspaces)
        if (d->title().starts_with("ores."))
            real_ts.push_back(d);
    const auto root = std::ranges::find_if(tspaces,
        [](const node* d) { return d->title() == "ores"; });
    if (root != tspaces.end()) {
        auto spaces = real_ts;
        std::ranges::stable_sort(spaces, {}, &node::title);
        std::vector<std::vector<std::string>> rows;
        for (const auto* d : spaces)
            rows.push_back({d->link(),
                std::to_string(count_of(facets_by_ts, d->title())),
                std::string(d->keyword("description"))});
        edits.push_back({*root, "Technical spaces", build_table(
            {"Technical space", "Facets", "Description"}, rows)});
    }
    for (const auto* ts : real_ts) {
        std::vector<std::vector<std::string>> rows;
        for (const auto* f : sorted_by_title(facets_by_ts, ts->title()))
            rows.push_back({f->link(),
                std::to_string(count_of(arch_by_facet, f->title())),
                std::string(f->keyword("description"))});
        edits.push_back({ts, "Facets", build_table(
            {"Facet", "Archetypes", "Description"}, rows)});
    }
    for (const auto* f : facets) {
        if (!f->title().starts_with("ores."))
            continue;
        std::vector<std::vector<std::string>> rows;
        for (const auto* a : sorted_by_title(arch_by_facet, f->title()))
            rows.push_back({a->link(), std::string(a->keyword("description"))});
        edits.push_back({f, "Archetypes",
            build_table({"Archetype", "Description"}, rows)});
    }

    std::map<const node*, std::string> texts;
    for (const auto& e : edits) {
        auto [it, fresh] = texts.try_emplace(e.doc);
        if (fresh)
            it->second = v.read_file(e.doc->path).value_or("");
        it->second = replace_section_table(it->second, e.heading, e.table);
    }
    for (const auto& d : docs)
        if (const auto it = texts.find(&d); it != texts.end())
            out.files.add(d.path, it->second);
}

}

const ct::descriptor physical_inventory{
    .name = "physical_inventory",
    .version = 1,
    .summary = "Writes the child tables of the physical-space documents.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = {},
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{inventories}};

}

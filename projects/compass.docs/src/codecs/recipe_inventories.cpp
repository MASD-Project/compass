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
#include "compass.docs/codecs/recipe_inventories.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

const pt::regex& link_re() {
    static const pt::regex re(R"(^- \[\[id:([^]]+)\]\[(.+?)\]\]\s*(.*)$)");
    return re;
}

const pt::regex& heading_re() {
    static const pt::regex re(R"(^\* (.+?)\s*$)");
    return re;
}

/**
 * @brief The frontmatter fields a recipe is listed by.
 */
struct recipe {
    std::string id;
    std::optional<std::string> title;
    std::optional<std::string> filetags;
};

/**
 * @brief How read_recipe takes a keyword seen twice: the shell script
 * keeps the first and stops at the first heading, the HTTP script keeps
 * the last and reads the whole file.
 */
enum class keywords : std::uint8_t { first_until_heading, last };

recipe read_recipe(std::string_view text, keywords mode) {
    static const pt::regex keyword(R"(^#\+(\w[\w-]*):\s*(.*)$)");
    recipe out;
    for (const auto line : co::split_lines(text)) {
        const auto stripped = co::strip(line);
        if (out.id.empty() && stripped.starts_with(":ID:"))
            out.id = co::strip(stripped.substr(4));
        if (const auto m = keyword.match(stripped)) {
            std::string key(m->group(1));
            std::ranges::transform(key, key.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            const std::string value(co::strip(m->group(2)));
            auto* slot = key == "title" ? &out.title
                : key == "filetags" ? &out.filetags : nullptr;
            if (slot != nullptr && (mode == keywords::last || !*slot))
                *slot = value;
        }
        if (mode == keywords::first_until_heading &&
            stripped.starts_with("* "))
            break;
    }
    return out;
}

/**
 * @brief The capture of @p re in the filetags of @p r, or nothing.
 */
std::optional<std::string> filetag_category(const recipe& r,
    const pt::regex& re) {
    if (!r.filetags)
        return std::nullopt;
    const auto m = re.search(*r.filetags);
    return m ? std::optional<std::string>(m->group(1)) : std::nullopt;
}

/**
 * @brief " ".join(word.capitalize() for word in category.split("_")), for
 * ASCII.
 */
std::string title_case(std::string_view category) {
    std::string out;
    bool first = true;
    for (const char c : category) {
        if (c == '_') {
            out.push_back(' ');
            first = true;
            continue;
        }
        const auto u = static_cast<unsigned char>(c);
        out.push_back(static_cast<char>(first ? std::toupper(u)
            : std::tolower(u)));
        first = false;
    }
    return out;
}

std::string lowered(std::string_view text) {
    std::string out(text);
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

/**
 * @brief True when @p text has a line git leaves in a file it could not
 * merge.
 */
bool has_conflict(std::string_view text) {
    static const pt::regex marker(R"(^(?:<{7}|>{7})(?: |$)|^={7}$)");
    return std::ranges::any_of(co::split_lines(text),
        [](std::string_view l) { return marker.match(l).has_value(); });
}

/**
 * @brief The files under @p dir ending .org, in pathlib's order.
 */
std::vector<std::string_view> org_files(const ct::corpus_view& v,
    std::string_view dir) {
    auto files = v.list_files(dir, ".org");
    std::ranges::stable_sort(files, co::component_less);
    return files;
}

/**
 * @brief Recipes grouped by category, the categories in the order they
 * were first seen, as a dict fills.
 */
struct grouping {
    std::vector<std::string> order;
    std::map<std::string, std::vector<recipe>, std::less<>> members;

    void add(const std::string& category, recipe r) {
        auto [it, fresh] = members.try_emplace(category);
        if (fresh)
            order.push_back(category);
        it->second.push_back(std::move(r));
    }
};

constexpr std::string_view shell_root = "doc/recipes/shell/";
constexpr std::string_view shell_index = "doc/recipes/shell/shell.org";

std::string shell_heading(std::string_view category) {
    if (category == "marketdata")
        return "Market data";
    if (category == "ore")
        return "ORE documents";
    if (category == "entity")
        return "Entity commands";
    if (category == "index")
        return "Inventory";
    return title_case(category);
}

std::string render_links(const std::vector<recipe>& recipes,
    const std::map<std::string, std::string, std::less<>>& notes) {
    std::string out;
    for (const auto& r : recipes) {
        if (!out.empty())
            out.push_back('\n');
        out += "- [[id:" + r.id + "][" + *r.title + "]]";
        const auto it = notes.find(r.id);
        if (it != notes.end() && !it->second.empty())
            out += " " + it->second;
    }
    return out;
}

struct section {
    std::string title;
    std::vector<std::string_view> prose;
};

std::string_view strip_newlines(std::string_view text) {
    while (text.starts_with('\n'))
        text.remove_prefix(1);
    while (text.ends_with('\n'))
        text.remove_suffix(1);
    return text;
}

void shell(const ct::corpus_view& v, ct::file_output& out) {
    static const pt::regex category_re(R"(:recipe:shell:([^:]+):)");
    const auto current = v.read_file(shell_index);
    if (!current) {
        co::refuse(out, shell_index, "the index does not exist",
            "Restore the index page.");
        return;
    }
    if (has_conflict(*current)) {
        co::refuse(out, shell_index, "the index holds merge conflict "
            "markers", "Resolve the conflict.");
        return;
    }

    grouping groups;
    for (const auto path : org_files(v, shell_root)) {
        if (path == shell_index)
            continue;
        auto r = read_recipe(v.read_file(path).value_or(""),
            keywords::first_until_heading);
        if (r.id.empty() || !r.title || r.title->empty())
            continue;
        auto category = filetag_category(r, category_re);
        if (!category) {
            const auto parent = co::parent_of(path);
            const auto name = parent.substr(parent.rfind('/') + 1);
            if (name != "shell")
                category = std::string(name);
        }
        if (!category || category->empty() || *category == "index")
            continue;
        groups.add(*category, std::move(r));
    }
    for (auto& [_, recipes] : groups.members)
        std::ranges::stable_sort(recipes, {}, [](const recipe& r) {
            return lowered(*r.title);
        });

    std::vector<std::string_view> head;
    std::vector<section> sections;
    std::map<std::string, std::string, std::less<>> notes;
    for (const auto line : co::split_lines(*current)) {
        if (const auto h = heading_re().match(line)) {
            sections.push_back({std::string(h->group(1)), {}});
        } else if (sections.empty()) {
            head.push_back(line);
        } else if (const auto l = link_re().match(line)) {
            notes[std::string(l->group(1))] = co::strip(l->group(3));
        } else {
            sections.back().prose.push_back(line);
        }
    }

    std::map<std::string, std::string, std::less<>> by_heading;
    for (const auto& category : groups.order)
        by_heading[shell_heading(category)] = category;

    auto join = [](const std::vector<std::string_view>& lines) {
        std::string s;
        for (std::size_t i = 0; i < lines.size(); ++i)
            (s += i > 0 ? "\n" : "") += lines[i];
        return s;
    };
    std::vector<std::string> blocks;
    auto head_text = join(head);
    while (head_text.ends_with('\n'))
        head_text.pop_back();
    blocks.push_back(head_text);
    std::vector<std::string> listed;
    for (const auto& s : sections) {
        const auto it = by_heading.find(s.title);
        if (it == by_heading.end())
            continue;
        listed.push_back(it->second);
        std::string block = "* " + s.title + "\n";
        const auto joined = join(s.prose);
        if (const auto prose = strip_newlines(joined); !prose.empty())
            (block += "\n") += std::string(prose) + "\n";
        if (const auto links = render_links(groups.members[it->second],
                notes); !links.empty())
            (block += "\n") += links;
        blocks.push_back(block);
    }
    for (const auto& [category, recipes] : groups.members) {
        if (std::ranges::find(listed, category) != listed.end())
            continue;
        blocks.push_back("* " + shell_heading(category) + "\n\n" +
            render_links(recipes, notes));
    }
    std::string text;
    for (std::size_t i = 0; i < blocks.size(); ++i)
        (text += i > 0 ? "\n\n" : "") += blocks[i];
    while (text.ends_with('\n'))
        text.pop_back();
    out.files.add(shell_index, text + "\n");
}

constexpr std::string_view http_root = "doc/recipes/http/";
constexpr std::string_view http_index =
    "doc/recipes/http/generated_recipes.org";
constexpr std::string_view http_frontmatter = R"(:PROPERTIES:
:ID: 3EB4B454-B837-41FA-8A8D-2D9C49DE967A
:END:
#+title: Generated HTTP recipes
#+description: Inventory of the generated HTTP recipes grouped by category.
#+type: knowledge
#+level: cross
#+filetags: :recipe:http:index:knowledge:
#+created: 2026-09-27
#+updated: 2026-09-27

The request recipes the =ores.doc.http-recipe= facet generates, one per
resource, listed by category. The link lists are generated by compass from the
recipes themselves; regenerate with =compass generate=, and a new recipe appears
here. The paragraph under each heading, and any note beside a link, are
hand-written and are kept.

The hand-written recipes that predate the facet are catalogued separately in
=doc/recipes/http/http.org=.)";

void http(const ct::corpus_view& v, ct::file_output& out) {
    static const pt::regex category_re(R"(:recipe:http:([^:]+):)");
    const auto current = v.read_file(http_index).value_or("");
    if (has_conflict(current)) {
        co::refuse(out, http_index, "the index holds merge conflict "
            "markers", "Resolve the conflict.");
        return;
    }

    std::map<std::string, std::string, std::less<>> paragraphs;
    std::map<std::string, std::string, std::less<>> notes;
    std::optional<std::string> heading;
    std::string body;
    std::size_t body_lines = 0;
    auto flush = [&] {
        if (!heading)
            return;
        const auto text = co::strip(body);
        if (!text.empty())
            paragraphs[*heading] = text;
    };
    for (const auto line : co::split_lines(current)) {
        if (const auto h = heading_re().match(line)) {
            flush();
            heading = std::string(h->group(1));
            body.clear();
            body_lines = 0;
            continue;
        }
        if (const auto l = link_re().match(line)) {
            notes[std::string(l->group(1))] = co::strip(l->group(3));
            continue;
        }
        if (heading)
            (body += body_lines++ > 0 ? "\n" : "") += line;
    }
    flush();

    grouping groups;
    for (const auto path : org_files(v, http_root)) {
        const auto rest = path.substr(http_root.size());
        const auto slash = rest.find('/');
        if (slash == std::string_view::npos ||
            rest.find('/', slash + 1) != std::string_view::npos)
            continue;
        auto r = read_recipe(v.read_file(path).value_or(""), keywords::last);
        const auto category = filetag_category(r, category_re);
        if (category && !category->empty() && !r.id.empty())
            groups.add(*category, std::move(r));
    }

    std::string text(http_frontmatter);
    text += "\n";
    for (const auto& [category, recipes] : groups.members) {
        const auto title = title_case(category);
        text += "\n* " + title + "\n\n";
        const auto p = paragraphs.find(title);
        if (p != paragraphs.end())
            text += p->second + "\n\n";
        for (const auto& r : recipes) {
            const auto n = notes.find(r.id);
            text += "- [[id:" + r.id + "][" + r.title.value_or("") + "]]";
            if (n != notes.end() && !n->second.empty())
                text += " " + n->second;
            text += "\n";
        }
    }
    while (text.ends_with('\n'))
        text.pop_back();
    out.files.add(http_index, text + "\n");
}

void inventories(const ct::corpus_view& v, ct::file_output& out) {
    shell(v, out);
    http(v, out);
}

}

const ct::descriptor recipe_inventories{
    .name = "recipe_inventories",
    .version = 2,
    .summary = "Writes the shell and HTTP recipe inventories.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = co::refusal_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{inventories}};

}

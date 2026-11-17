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
#include "compass.docs/codecs/pattern_uses.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::uint32_t unknown_use = 0;
constexpr std::uint32_t one_way = 1;
constexpr std::uint32_t not_a_pattern = 2;
constexpr std::uint32_t shared_id = 3;

constexpr std::array<std::string_view, 1> named_patterns_sources{
    "470D4571-47A4-4AEF-9A93-2B4DE0279C1E"};

constexpr std::array<ct::rule, 4> rules{{
    {"unknown_use", "Every page a pattern lists under In ORE Studio "
        "exists."},
    {"one_way", "A pattern and each of its uses link each other: the use "
        "links the pattern, and the pattern lists the use.", named_patterns_sources},
    {"not_a_pattern", "A page lists only service architecture patterns "
        "under Patterns used.", named_patterns_sources},
    {"shared_id", "No two pages of a pattern link share an ID."}}};

constexpr std::string_view pattern_type = "service_architecture_pattern";
constexpr std::array<std::string_view, 3> exempt_prefixes{"doc/plans/",
    "doc/knowledge/external/", "doc/agile/"};

std::string upper(std::string_view s) {
    std::string out(s);
    for (auto& c : out) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    }
    return out;
}

/**
 * @brief A page: its path and text, read as the script reads it.
 */
struct page {
    std::string_view path;
    std::string text;
};

std::string_view section(std::string_view text, std::string_view title) {
    static const pt::regex heading(R"(^\* (.+?)\s*$)", {.multiline = true});
    const auto heads = heading.find_all(text);
    for (std::size_t i = 0; i < heads.size(); ++i) {
        if (heads[i].group(1) != title)
            continue;
        const auto start = heads[i].start();
        const auto end = i + 1 < heads.size() ? heads[i + 1].start()
                                              : text.size();
        return text.substr(start, end - start);
    }
    return {};
}

std::vector<std::string> links(std::string_view body) {
    static const pt::regex link(R"(\[\[id:([0-9A-Fa-f-]{36})\]\[[^\]]*\]\])");
    std::vector<std::string> out;
    for (const auto& m : link.find_all(body))
        out.push_back(upper(m.group(1)));
    return out;
}

bool contains(const std::vector<std::string>& ids, std::string_view id) {
    return std::ranges::find(ids, id) != ids.end();
}

bool exempt(std::string_view path) {
    return std::ranges::any_of(exempt_prefixes, [&](auto p) {
        return path.starts_with(p);
    });
}

bool is_searched(std::string_view path) {
    if (!path.starts_with("doc/") && !path.starts_with("projects/"))
        return false;
    const std::string rooted = "/" + std::string(path);
    return !rooted.contains("/build/") && !rooted.contains("/vcpkg/");
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex id_line(R"(^:ID:\s*(\S+)\s*$)",
        {.multiline = true});
    static const pt::regex type_line(R"(^#\+type:\s*(\S+)\s*$)",
        {.multiline = true});

    std::map<std::string, page> docs;
    std::map<std::string, std::vector<std::string_view>> duplicates;
    std::map<std::string, std::string> types;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto d = v[i];
        if (!is_searched(d.path) || !pt::is_utf8(d.source))
            continue;
        auto text = pt::universal_newlines(d.source);
        const auto id = id_line.search(text);
        if (!id)
            continue;
        const auto key = upper(id->group(1));
        if (const auto it = docs.find(key); it != docs.end()) {
            auto& shared = duplicates[key];
            if (shared.empty())
                shared.push_back(it->second.path);
            shared.push_back(d.path);
        }
        const auto type = type_line.search(text);
        types[key] = type ? std::string(type->group(1)) : std::string();
        docs[key] = {d.path, std::move(text)};
    }

    std::set<std::string> patterns;
    for (const auto& [id, type] : types) {
        if (type == pattern_type)
            patterns.insert(id);
    }
    std::set<std::string> involved = patterns;
    for (const auto& pid : patterns) {
        const auto& p = docs.at(pid);
        for (const auto& uid : links(section(p.text, "In ORE Studio"))) {
            const auto use = docs.find(uid);
            if (use == docs.end()) {
                co::report(out, p.path, 0, unknown_use, std::string(p.path) +
                    " lists id " + uid + " under In ORE Studio, which no "
                    "page has", "Link a page that exists, or drop the row.");
                continue;
            }
            involved.insert(uid);
            if (patterns.contains(uid) || exempt(use->second.path))
                continue;
            if (!contains(links(use->second.text), pid))
                co::report(out, use->second.path, 0, one_way,
                    std::string(use->second.path) + " is a use listed by " +
                    std::string(p.path) + " but does not link the pattern "
                    "back", "List the pattern under the use's Patterns "
                    "used.");
        }
    }
    for (const auto& [did, d] : docs) {
        if (exempt(d.path) || patterns.contains(did))
            continue;
        const auto used = links(section(d.text, "Patterns used"));
        if (!used.empty())
            involved.insert(did);
        for (const auto& pid : used) {
            if (!patterns.contains(pid)) {
                const auto target = docs.find(pid);
                co::report(out, d.path, 0, not_a_pattern, std::string(d.path) +
                    " lists " + (target == docs.end() ? pid
                        : std::string(target->second.path)) + " under "
                    "Patterns used, which is not a service architecture "
                    "pattern", "List only service architecture patterns "
                    "there.");
                continue;
            }
            const auto& p = docs.at(pid);
            if (!contains(links(section(p.text, "In ORE Studio")), did))
                co::report(out, d.path, 0, one_way, std::string(d.path) +
                    " uses " + std::string(p.path) + ", which does not list "
                    "it under In ORE Studio", "Add the use to the pattern's "
                    "In ORE Studio section.");
        }
    }
    for (const auto& [did, paths] : duplicates) {
        if (!involved.contains(did))
            continue;
        std::string all;
        for (const auto p : paths)
            all += (all.empty() ? "" : ", ") + std::string(p);
        co::report(out, paths.front(), 0, shared_id, "id " + did +
            " is shared by " + all, "Give each page an ID of its own.");
    }
}

}

const ct::descriptor pattern_uses{
    .name = "pattern_uses",
    .version = 1,
    .summary = "Reports each service architecture pattern and use that do "
        "not link each other.",
    .reads = ct::inputs::every_document(),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}

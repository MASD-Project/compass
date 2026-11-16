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
#include "compass.cpp/codecs/core_link_edges.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 3> rules{{
    {"forbidden_edge", "No component's core links another component's core "
        "or service library; it links the api contract."},
    {"unresolved_link", "Every library a core links names no variable the "
        "check cannot resolve."},
    {"unnamed_core", "Every core library file states its name."}}};

const pt::regex& name_setting() {
    static const pt::regex r(R"re(set\(\s*name\s+"?([A-Za-z0-9_.]+)"?\s*\))re");
    return r;
}

/**
 * @brief The file without its comments: a line that starts with # left
 * empty rather than dropped, so lines keep their numbers, and the rest of a
 * line after a # cut.
 */
std::string without_comments(std::string_view text) {
    std::string out;
    bool first = true;
    for (const auto line : ops::split_lines(text)) {
        if (!first)
            out.push_back('\n');
        first = false;
        if (ops::strip(line).starts_with('#'))
            continue;
        out.append(line.substr(0, line.find('#')));
    }
    return out;
}

struct word {
    std::string text;
    std::size_t offset;
};

/**
 * @brief The words of the argument text of every target_link_libraries
 * call, with where each starts.
 */
std::vector<std::vector<word>> link_calls(std::string_view text) {
    static const pt::regex call(R"(target_link_libraries\s*\()");
    std::vector<std::vector<word>> out;
    for (const auto& m : call.find_all(text)) {
        int depth = 1;
        auto at = m.end();
        while (at < text.size() && depth != 0) {
            if (text[at] == '(')
                ++depth;
            else if (text[at] == ')')
                --depth;
            ++at;
        }
        const auto end = at - 1;
        std::vector<word> words;
        for (auto i = m.end(); i < end;) {
            i = ops::skip_spaces(text, i);
            auto j = i;
            while (j < end && !ops::is_space(text[j]))
                ++j;
            if (j > i) {
                std::string w(text.substr(i, j - i));
                if (w != "PUBLIC" && w != "PRIVATE" && w != "INTERFACE")
                    words.push_back({std::move(w), i});
            }
            i = j;
        }
        out.push_back(std::move(words));
    }
    return out;
}

std::string resolve(std::string w, const std::string& component) {
    const std::array<std::pair<std::string_view, std::string>, 4> variables{{
        {"${lib_target_name}", component + ".lib"},
        {"${lib_binary_name}", component},
        {"${name}", component},
        {"${component}", component}}};
    for (const auto& [variable, value] : variables) {
        for (auto at = w.find(variable); at != std::string::npos;
             at = w.find(variable, at + value.size()))
            w.replace(at, variable.size(), value);
    }
    return w;
}

std::vector<std::string> split_dots(std::string_view s) {
    std::vector<std::string> parts;
    for (std::size_t start = 0;;) {
        const auto dot = s.find('.', start);
        parts.emplace_back(s.substr(start, dot - start));
        if (dot == std::string_view::npos)
            return parts;
        start = dot + 1;
    }
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::vector<std::string_view> files;
    for (const auto p : v.list_files("projects/", "CMakeLists.txt")) {
        if (p.ends_with("/CMakeLists.txt"))
            files.push_back(p);
    }
    std::ranges::sort(files, ops::component_less);
    std::vector<std::pair<std::string_view, std::string>> texts;
    std::set<std::string> components;
    for (const auto p : files) {
        const auto bytes = v.read_file(p).value_or("");
        auto text = without_comments(bytes);
        for (const auto& m : name_setting().find_all(text)) {
            const auto parts = split_dots(m.group(1));
            if (parts.size() == 3 && parts[0] == "ores")
                components.insert(parts[1]);
        }
        texts.emplace_back(p, std::move(text));
    }

    std::set<std::pair<std::string, std::string>> seen;
    for (const auto& [path, text] : texts) {
        const auto named = name_setting().search(text);
        if (!named)
            continue;
        const std::string component(named->group(1));
        if (!component.starts_with("ores.") || !component.ends_with(".core"))
            continue;
        const auto core = component + ".lib";
        const auto own = split_dots(component)[1];
        const auto bytes = v.read_file(path).value_or("");
        for (const auto& words : link_calls(text)) {
            if (words.empty() || resolve(words[0].text, component) != core)
                continue;
            for (std::size_t i = 1; i < words.size(); ++i) {
                const auto links = resolve(words[i].text, component);
                if (!seen.insert({core, links}).second)
                    continue;
                const auto at = ops::offset_of_line(bytes,
                    ops::line_of(text, words[i].offset));
                if (links.find('$') != std::string::npos) {
                    ops::report(out, path, at, 1, core + " links '" + links +
                        "', which names a variable the check cannot "
                        "resolve", "name the library");
                    continue;
                }
                const auto parts = split_dots(links);
                if (parts.size() != 4 || parts[0] != "ores")
                    continue;
                if ((parts[2] != "core" && parts[2] != "service") ||
                    !components.contains(parts[1]) || parts[1] == own)
                    continue;
                ops::report(out, path, at, 0, core + " links " + links +
                    ", which is " + parts[1] + "'s " + parts[2] + " library",
                    "link the component's api contract, not its " +
                        parts[2]);
            }
        }
    }

    for (const auto& [path, text] : texts) {
        const auto parts = ops::components_of(path);
        if (parts.size() == 5 && parts[2] == "core" && parts[3] == "src" &&
            !name_setting().search(text))
            ops::report(out, path, 0, 2, "a core library file states no "
                "name, so the check cannot tell which core it builds",
                "set(name ...) the core's name");
    }
}

}

const ct::descriptor core_link_edges{
    .name = "core_link_edges",
    .version = 1,
    .summary = "Reports each component core that links another component's "
        "core or service library.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}

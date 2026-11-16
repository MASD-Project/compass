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
#include "compass.cpp/codecs/unity_collisions.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
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

constexpr std::array<ct::rule, 2> rules{{
    {"unrecorded_collision", "No two sources of a target define the same "
        "file-local name outside the unity exclusions."},
    {"missing_recorded_file",
        "Every file the unity exclusions name exists."}}};

constexpr std::string_view exclusions = "cmake/unity_exclusions.cmake";

constexpr std::array<std::string_view, 11> not_a_declaration{"#", "}",
    "using ", "template", "class ", "struct ", "enum ", "namespace", "return",
    "typedef", "extern"};

const pt::regex& lexeme() {
    static const pt::regex r(
        R"re(R"\((?:.|\n)*?\)"|//[^\n]*|/\*.*?\*/|"(?:[^"\\\n]|\\.)*"|'(?:[^'\\\n]|\\.)*')re",
        {.dotall = true});
    return r;
}

const pt::regex& declaration() {
    static const pt::regex r(
        R"(^(?P<quals>(?:(?:inline|static|constexpr|const|thread_local)\s+)*))"
        R"((?:[\w:<>,\s\*&\[\]]+?)\s*\b(?P<name>[A-Za-z_]\w*)\s*(?:\(|=|\{|;))");
    return r;
}

const pt::regex& brace() {
    static const pt::regex r(
        R"(\bnamespace\s+([A-Za-z_][\w:]*)?\s*\{|\{|\})");
    return r;
}

/**
 * @brief An internal-linkage name: its enclosing named namespaces, the
 * name, and the first line that defines it.
 */
using name_key = std::pair<std::string, std::string>;

std::map<name_key, std::uint32_t> internal_names(std::string_view text) {
    const auto blanked = lexeme().substitute(text, [](const auto& m) {
        return pt::blank_keeping_newlines(m.group());
    });
    // A namespace name per open namespace, empty for an anonymous one.
    std::vector<std::optional<std::string>> stack;
    std::vector<int> scope_depths;
    int depth = 0;
    long parens = 0;
    std::map<name_key, std::uint32_t> found;
    std::uint32_t number = 0;
    std::size_t start = 0;
    while (start <= blanked.size()) {
        auto end = blanked.find('\n', start);
        if (end == std::string::npos)
            end = blanked.size();
        const std::string_view line(blanked.data() + start, end - start);
        start = end + 1;
        ++number;
        const auto stripped = ops::strip(line);
        const bool at_namespace_scope = depth == (scope_depths.empty()
            ? 0 : scope_depths.back()) && parens == 0;
        if (!stripped.starts_with('#'))
            parens += static_cast<long>(ops::count(line, "(")) -
                static_cast<long>(ops::count(line, ")"));
        const bool excluded = std::ranges::any_of(not_a_declaration,
            [&](auto p) { return stripped.starts_with(p); });
        if (at_namespace_scope && !stripped.empty() && !excluded) {
            const auto m = declaration().match(stripped);
            if (m && !stripped.substr(0, m->start("name")).ends_with("::")) {
                std::set<std::string_view> quals;
                const auto q = m->group("quals");
                for (std::size_t i = 0; i < q.size();) {
                    i = ops::skip_spaces(q, i);
                    auto j = i;
                    while (j < q.size() && !ops::is_space(q[j]))
                        ++j;
                    if (j > i)
                        quals.insert(q.substr(i, j - i));
                    i = j;
                }
                const bool anonymous = std::ranges::any_of(stack,
                    [](const auto& n) { return !n.has_value(); });
                const bool constant = (quals.contains("const") ||
                    quals.contains("constexpr")) && !quals.contains("inline");
                if (anonymous || quals.contains("static") || constant) {
                    std::string ns;
                    for (const auto& n : stack) {
                        if (!n || n->empty())
                            continue;
                        if (!ns.empty())
                            ns += "::";
                        ns += *n;
                    }
                    found.try_emplace({ns, std::string(m->group("name"))},
                        number);
                }
            }
        }
        for (const auto& m : brace().find_all(line)) {
            const auto token = m.group();
            if (token.starts_with("namespace")) {
                stack.emplace_back(m.group_if(1)
                    ? std::optional<std::string>(m.group(1)) : std::nullopt);
                ++depth;
                scope_depths.push_back(depth);
            } else if (token == "{") {
                ++depth;
            } else {
                if (!scope_depths.empty() && depth == scope_depths.back()) {
                    scope_depths.pop_back();
                    stack.pop_back();
                }
                --depth;
            }
        }
    }
    return found;
}

/**
 * @brief True for a .cpp under projects/ores.<name>/ outside build and
 * node_modules.
 */
bool is_scanned(std::string_view path) {
    const auto parts = ops::components_of(path);
    if (parts.size() < 3 || !parts[1].starts_with("ores."))
        return false;
    return std::ranges::none_of(parts, [](auto p) {
        return p == "build" || p == "node_modules";
    });
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::vector<std::string_view> sources;
    for (const auto p : v.list_files("projects/ores.", ".cpp")) {
        if (is_scanned(p))
            sources.push_back(p);
    }
    std::ranges::sort(sources, ops::component_less);

    std::unordered_map<std::string, std::string> target_of_dir;
    const auto target_dir = [&](std::string_view source) {
        std::string dir(ops::parent_of(source));
        const auto cached = target_of_dir.find(dir);
        if (cached != target_of_dir.end())
            return cached->second;
        auto path = dir;
        while (!v.file_exists(path.empty() ? "CMakeLists.txt"
                : path + "/CMakeLists.txt") && !path.empty())
            path = std::string(ops::parent_of(path));
        target_of_dir.emplace(dir, path);
        return path;
    };

    using group_key = std::tuple<std::string, std::string, std::string>;
    struct site {
        std::string_view file;
        std::uint32_t line;
    };
    std::map<group_key, std::vector<site>> groups;
    std::unordered_map<std::string_view, std::string> texts;
    for (const auto p : sources) {
        const auto bytes = v.read_file(p).value_or("");
        if (!ops::is_utf8(bytes))
            continue;
        const auto text = ops::universal_newlines(bytes);
        const auto target = target_dir(p);
        for (const auto& [key, line] : internal_names(text))
            groups[{target, key.first, key.second}].push_back({p, line});
        texts.emplace(p, bytes);
    }

    const auto recorded_text = v.read_file(exclusions);
    std::set<std::string, std::less<>> have;
    if (recorded_text) {
        static const pt::regex entry(
            R"re("\$\{CMAKE_SOURCE_DIR\}/([^"]+\.cpp)")re");
        for (const auto& m : entry.find_all(*recorded_text))
            have.emplace(m.group(1));
    }

    for (const auto& [key, sites] : groups) {
        if (sites.size() < 2)
            continue;
        std::vector<site> kept;
        for (const auto& s : sites) {
            if (!have.contains(s.file))
                kept.push_back(s);
        }
        if (kept.size() < 2)
            continue;
        const auto& [target, ns, name] = key;
        std::string files;
        for (const auto& s : kept) {
            if (!files.empty())
                files += ' ';
            files += s.file;
        }
        const auto& first = kept.front();
        ops::report(out, first.file,
            ops::offset_of_line(texts.at(first.file), first.line), 0,
            (ns.empty() ? std::string("(global)") : ns) + "::" + name +
                " has internal linkage in " + files + ", which a unity "
                "build of " + target + " merges",
            "name the helper for what it does, or share one definition; "
            "a collision the compiler alone sees is recorded in " +
                std::string(exclusions));
    }

    for (const auto& f : have) {
        if (v.file_exists(f))
            continue;
        const auto at = recorded_text->find(f);
        ops::report(out, exclusions, static_cast<std::uint32_t>(at), 1,
            "the unity exclusions name " + f + ", which does not exist",
            "remove the file from the unity exclusions");
    }
}

}

const ct::descriptor unity_collisions{
    .name = "unity_collisions",
    .version = 1,
    .summary = "Reports file-local names that two sources of one target "
        "define, which a unity build merges.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}

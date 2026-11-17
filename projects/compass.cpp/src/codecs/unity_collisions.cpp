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
#include <span>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/declarations.hpp"
#include "compass.cpp/ops/lexer.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<std::string_view, 1> unrecorded_collision_sources{
    "6C744720-FF73-4895-92E3-24E1E9BF518E"};

constexpr std::array<ct::rule, 2> rules{{
    {"unrecorded_collision", "No two sources of a target define the same "
        "file-local name outside the unity exclusions.",
        unrecorded_collision_sources},
    {"missing_recorded_file",
        "Every file the unity exclusions name exists."}}};

constexpr std::string_view exclusions = "cmake/unity_exclusions.cmake";

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

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    std::vector<std::string_view> sources;
    for (const auto p : v.list_files("projects/ores.", ".cpp")) {
        if (is_scanned(p))
            sources.push_back(p);
    }
    std::ranges::sort(sources, ops::component_less);
    return sources;
}

/**
 * @brief The fact of a source: each name of internal linkage, its
 * namespace and where the line its declaration starts on begins, read from
 * its model.
 */
void map(std::string_view, std::string_view bytes, const ct::option_values&,
    ct::file_scan& out) {
    const auto lexed = ops::lex(bytes);
    ct::fact_writer fact(out.fact);
    for (const auto& [key, start] :
         ops::internal_names(ops::model_of(bytes, lexed)))
        fact.text(key.first).text(key.second)
            .number(ops::offset_of_line(bytes, ops::line_of(bytes, start)));
}

void reduce(std::span<const ct::scanned_file> sources,
    const ct::corpus_view& v, ct::diagnostic_table& out) {

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
        std::uint32_t offset;
    };
    std::map<group_key, std::vector<site>> groups;
    for (const auto& [p, f] : sources) {
        ct::fact_reader fact(f);
        const auto target = target_dir(p);
        while (!fact.done()) {
            std::string ns(fact.text());
            std::string name(fact.text());
            const auto offset = fact.number();
            groups[{target, std::move(ns), std::move(name)}].push_back(
                {p, offset});
        }
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
        ops::report(out, first.file, first.offset, 0,
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
    .version = 3,
    .summary = "Reports file-local names that two sources of one target "
        "define, which a unity build merges.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}

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
#include "compass.sql/codecs/sql_reachability.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"
#include "compass.sql/ops/scripts.hpp"

namespace compass::sql::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

constexpr std::array<ct::rule, 2> rules{{
    {"unreachable_file", "Every .sql file under create, populate and drop "
        "is reached from a schema entry point."},
    {"missing_include", "Every include and declared entry point names a "
        "file that exists."}}};

constexpr std::string_view sql_root = "projects/ores.sql";

constexpr std::array<std::string_view, 4> extra_entry_points{
    "drop/drop.sql", "drop/drop_database.sql", "drop/drop_test_databases.sql",
    "populate/ip2country/ip2country_import.sql"};

constexpr std::array<std::string_view, 3> trees{"create", "populate", "drop"};

/**
 * @brief @p name from directory @p base, with "." and empty components
 * dropped and ".." taken lexically, as resolve() leaves a path with no
 * symbolic link; nothing when it climbs out of the checkout.
 */
std::optional<std::string> join(std::string_view base, std::string_view name) {
    std::vector<std::string_view> parts;
    const auto add = [&](std::string_view path) {
        for (std::size_t start = 0; start <= path.size();) {
            auto end = path.find('/', start);
            if (end == std::string_view::npos)
                end = path.size();
            const auto part = path.substr(start, end - start);
            start = end + 1;
            if (part.empty() || part == ".")
                continue;
            if (part == "..") {
                if (parts.empty())
                    return false;
                parts.pop_back();
                continue;
            }
            parts.push_back(part);
        }
        return true;
    };
    if (name.starts_with('/') || !add(base) || !add(name))
        return std::nullopt;
    std::string out;
    for (const auto p : parts) {
        if (!out.empty())
            out += '/';
        out += p;
    }
    return out;
}

void add(ct::diagnostic_table& out, std::string_view file,
    std::uint32_t offset, std::uint32_t rule, const std::string& message,
    std::string_view fix) {
    out.add({.document = compass::org::types::document_id::none(),
        .range = {offset, offset},
        .rule = ct::rule_id{rule},
        .level = ct::severity::error,
        .message = message,
        .fix = fix,
        .file = file});
}

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    return v.list_files(std::string(sql_root) + "/", ".sql");
}

/**
 * @brief The fact of a file: each \i or \ir line, whether it is \ir, the
 * path it names and the offset of its backslash.
 */
void includes_of(std::string_view bytes, std::string& out) {
    static const pt::regex include_re(R"(^\s*\\(i(?:r)?)\s+(\S+))",
        {.multiline = true});
    const auto text = pt::decode_ignoring(bytes);
    ct::fact_writer fact(out);
    for (const auto& m : include_re.find_all(text))
        fact.number(m.group(1) == "ir" ? 1 : 0).text(m.group(2))
            .number(static_cast<std::uint32_t>(m.start(1) - 1));
}

void map(std::string_view, std::string_view bytes, const ct::option_values&,
    ct::file_scan& out) {
    includes_of(bytes, out.fact);
}

void reduce(std::span<const ct::scanned_file> files,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::unordered_map<std::string_view, std::string_view> facts;
    for (const auto& f : files)
        facts.emplace(f.path, f.fact);
    const std::string root(sql_root);
    std::vector<std::string> stack;
    for (const auto& f : files) {
        if (f.path.find('/', root.size() + 1) == std::string_view::npos)
            stack.emplace_back(f.path);
    }
    std::ranges::sort(stack, ops::component_less);
    const std::string_view fix = "include the file from its tree's "
        "aggregator in dependency order, or declare it an entry point";
    for (const auto name : extra_entry_points) {
        const auto path = root + "/" + std::string(name);
        if (v.file_exists(path))
            stack.push_back(path);
        else
            add(out, sql_root, 0, 1, "the entry point " + std::string(name) +
                " does not exist", "declare the entry point under its new "
                "name, or drop it");
    }
    std::set<std::string, std::less<>> seen;
    std::string read;
    while (!stack.empty()) {
        auto path = std::move(stack.back());
        stack.pop_back();
        if (seen.contains(path))
            continue;
        std::string_view fact;
        if (const auto it = facts.find(path); it != facts.end()) {
            fact = it->second;
        } else {
            read.clear();
            includes_of(v.read_file(path).value_or(""), read);
            fact = read;
        }
        const auto dir = path.substr(0, path.rfind('/'));
        ct::fact_reader includes(fact);
        while (!includes.done()) {
            const bool relative = includes.number() == 1;
            const auto entry = includes.text();
            const auto at = includes.number();
            const auto target = join(relative ? std::string_view(dir)
                : sql_root, entry);
            if (target && v.file_exists(*target)) {
                stack.push_back(*target);
                continue;
            }
            add(out, path, at, 1, "includes " + std::string(entry) +
                ", which does not exist; psql aborts the build on it",
                "correct the path or add the file");
        }
        seen.insert(std::move(path));
    }
    for (const auto tree : trees) {
        for (const auto p : v.list_files(root + "/" + std::string(tree) + "/",
                 ".sql")) {
            if (!seen.contains(p))
                add(out, p, 0, 0, "nothing reached from a schema entry point "
                    "includes this file", fix);
        }
    }
}

}

const ct::descriptor sql_reachability{
    .name = "sql_reachability",
    .version = 2,
    .summary = "Reports each SQL file no schema entry point reaches and "
        "each include that names no file.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}

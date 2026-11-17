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
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.sql/ops/includes.hpp"
#include "compass.sql/ops/lexer.hpp"
#include "compass.sql/ops/scripts.hpp"
#include "compass.sql/ops/sql_model.hpp"

namespace compass::sql::codecs {

namespace {

namespace ct = compass::codec::types;

constexpr std::array<ct::rule, 2> rules{{
    {"unreachable_file", "Every .sql file under create, populate and drop "
        "is reached from a schema entry point."},
    {"missing_include", "Every include and declared entry point names a "
        "file that exists."}}};

constexpr std::array<std::string_view, 3> trees{"create", "populate", "drop"};

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
    auto paths = v.list_files(std::string(ops::sql_root) + "/", ".sql");
    std::ranges::sort(paths, ops::component_less);
    return paths;
}

/**
 * @brief The fact of a file: its includes, in order.
 */
void map(std::string_view, std::string_view bytes, const ct::option_values&,
    ct::file_scan& out) {
    ct::fact_writer fact(out.fact);
    ops::write_includes(fact, ops::includes_of(bytes, ops::lex(bytes)));
}

void reduce(std::span<const ct::scanned_file> files,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::unordered_map<std::string_view, std::string_view> facts;
    std::vector<std::string_view> roots;
    for (const auto& f : files) {
        facts.emplace(f.path, f.fact);
        if (f.path.find('/', ops::sql_root.size() + 1) ==
            std::string_view::npos)
            roots.push_back(f.path);
    }
    const auto walk = ops::walk_includes(roots,
        [&](std::string_view path) {
            if (const auto it = facts.find(path); it != facts.end()) {
                ct::fact_reader fact(it->second);
                return ops::read_includes(fact);
            }
            const auto bytes = v.read_file(path).value_or("");
            return ops::includes_of(bytes, ops::lex(bytes));
        },
        [&](std::string_view path) { return v.file_exists(path); });
    for (const auto& name : walk.missing_entry_points)
        add(out, ops::sql_root, 0, 1, "the entry point " + name +
            " does not exist", "declare the entry point under its new "
            "name, or drop it");
    for (const auto& m : walk.missing_includes)
        add(out, m.file, m.offset, 1, "includes " + m.names +
            ", which does not exist; psql aborts the build on it",
            "correct the path or add the file");
    const std::string_view fix = "include the file from its tree's "
        "aggregator in dependency order, or declare it an entry point";
    for (const auto tree : trees) {
        for (const auto p : v.list_files(std::string(ops::sql_root) + "/" +
                 std::string(tree) + "/", ".sql")) {
            if (!walk.reached(p))
                add(out, p, 0, 0, "nothing reached from a schema entry point "
                    "includes this file", fix);
        }
    }
}

}

const ct::descriptor sql_reachability{
    .name = "sql_reachability",
    .version = 3,
    .summary = "Reports each SQL file no schema entry point reaches and "
        "each include that names no file.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}

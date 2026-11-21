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
#include "compass.sql/codecs/sql_hygiene.hpp"

#include <array>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.sql/ops/hygiene.hpp"

namespace compass::sql::codecs {

namespace {

namespace ct = compass::codec::types;

constexpr std::array<ct::rule, 4> rules{{
    {"unterminated_block_comment",
        "Every block comment closes; PostgreSQL nests them.",
        {}, ct::blame_method::line},
    {"duplicate_index", "No file creates the same index twice.",
        {}, ct::blame_method::line},
    {"comment_splits_literals",
        "No comment stands between two string literals.",
        {}, ct::blame_method::line},
    {"not_re_runnable", "A second run of the file succeeds.",
        {}, ct::blame_method::line}}};

constexpr std::array<ct::option, 1> options{{
    {"roots", "projects/ores.sql",
        "The directories whose .sql files are checked, comma-separated."}}};

void add(ct::diagnostic_table& out, std::string_view file,
    std::uint32_t at, std::uint32_t rule, const std::string& message,
    std::string_view fix) {
    out.add({.document = compass::org::types::document_id::none(),
        .range = {at, at},
        .rule = ct::rule_id{rule},
        .level = ct::severity::error,
        .message = message,
        .fix = fix,
        .file = file});
}

void check(std::string_view file, std::string_view text,
    ct::diagnostic_table& out) {
    const auto tokens = ops::lex(text);
    if (tokens.open_depth > 0)
        add(out, file, tokens.open_offset, 0,
            "a block comment is still open at the end of the file, so "
            "everything after it is comment", "close the comment; "
            "PostgreSQL nests block comments, so an inner /* needs its "
            "own */");
    for (const auto& d : ops::duplicate_indexes(ops::model_of(text, tokens)))
        add(out, file, d.offset, 1, "index " + d.name + " on " + d.table +
            " (" + d.columns + ") is defined more than once",
            "create the index once, outside any per-column loop");
    for (const auto at : ops::split_concatenations(text, tokens))
        add(out, file, at, 2, "a comment splits two string literals; "
            "PostgreSQL joins them only across whitespace",
            "move the comment above the statement");
    for (const auto& r : ops::not_re_runnables(text, tokens))
        add(out, file, r.offset, 3, r.why + "; a second run would fail",
            "make the statement idempotent");
}

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    std::vector<std::string_view> out;
    const auto roots = v.option("roots");
    for (std::size_t start = 0; start <= roots.size();) {
        auto end = roots.find(',', start);
        if (end == std::string_view::npos)
            end = roots.size();
        auto root = std::string(roots.substr(start, end - start));
        start = end + 1;
        if (root.empty())
            continue;
        if (!root.ends_with('/'))
            root.push_back('/');
        const auto paths = v.list_files(root, ".sql");
        out.insert(out.end(), paths.begin(), paths.end());
    }
    return out;
}

void map(std::string_view path, std::string_view bytes,
    const ct::option_values&, ct::file_scan& out) {
    check(path, bytes, out.findings);
}

}

const ct::descriptor sql_hygiene{
    .name = "sql_hygiene",
    .version = 3,
    .summary = "Reports unterminated comments, duplicate indexes, comments "
        "that split string literals and DDL that cannot run twice.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, nullptr}};

}

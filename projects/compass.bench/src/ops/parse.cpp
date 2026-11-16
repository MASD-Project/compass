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
#include "compass.bench/ops/parse.hpp"

#include <tree_sitter/tree-sitter-cpp.h>
#include <tree_sitter/tree-sitter-sql.h>
#include "compass.bench/ops/include_scan.hpp"

namespace compass::bench::ops {

std::optional<types::language> language_of(std::string_view extension) {
    if (is_scanned_extension(extension))
        return types::language::cpp;
    if (extension == ".sql")
        return types::language::sql;
    return std::nullopt;
}

std::string_view name_of(types::language l) {
    switch (l) {
    case types::language::cpp: return "cpp";
    case types::language::sql: return "sql";
    }
    return {};
}

const TSLanguage* grammar_of(types::language l) {
    switch (l) {
    case types::language::cpp: return tree_sitter_cpp();
    case types::language::sql: return tree_sitter_sql();
    }
    return nullptr;
}

std::uint32_t count_error_nodes(TSNode node) {
    std::uint32_t count = 0;
    TSTreeCursor cursor = ts_tree_cursor_new(node);
    for (;;) {
        const TSNode current = ts_tree_cursor_current_node(&cursor);
        bool descended = false;
        if (ts_node_has_error(current)) {
            if (ts_node_is_error(current) || ts_node_is_missing(current))
                ++count;
            descended = ts_tree_cursor_goto_first_child(&cursor);
        }
        if (descended)
            continue;
        while (!ts_tree_cursor_goto_next_sibling(&cursor)) {
            if (!ts_tree_cursor_goto_parent(&cursor)) {
                ts_tree_cursor_delete(&cursor);
                return count;
            }
        }
    }
}

}

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
#ifndef COMPASS_MODEL_OPS_OUTLINE_HPP
#define COMPASS_MODEL_OPS_OUTLINE_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "compass.org/types/handle.hpp"
#include "compass.org/types/tables.hpp"

namespace compass::model::ops {

using node_id = compass::org::types::handle<struct node_tag>;

/**
 * @brief A model document as ores.codegen's parse_org sees it: the document
 * and its headings as nodes, and per node its properties, prose lines, named
 * source blocks, tables and bullet lists.
 *
 * It is built from the compass.org tables, which say where each heading,
 * property drawer, source block, comment block and table is. The text of
 * each is read from the source by the rules parse_org applies to a line, so
 * a value comes out exactly as ores.codegen reads it. Every string_view
 * points into the source.
 *
 * Nodes are in pre-order and node 0 is the document. The children of a node
 * start at the next node and each following sibling starts at the
 * subtree_ends of the one before. Every other table lists its rows in
 * document order with the node each belongs to. A key written twice keeps
 * the last value, as a Python dict does.
 */
struct outline {
    std::vector<std::uint16_t> levels;
    std::vector<std::string_view> titles;
    std::vector<node_id> subtree_ends;
    std::vector<std::uint32_t> body_firsts;
    std::vector<std::string_view> body_lines;

    std::vector<std::string_view> frontmatter_keys;
    std::vector<std::string_view> frontmatter_values;
    std::vector<std::string_view> file_property_keys;
    std::vector<std::string_view> file_property_values;

    std::vector<node_id> property_nodes;
    std::vector<std::string_view> property_keys;
    std::vector<std::string_view> property_values;

    std::vector<node_id> block_nodes;
    std::vector<std::string_view> block_names;
    std::vector<std::string_view> block_languages;
    std::vector<std::string_view> block_implements;
    std::vector<std::string_view> block_codes;

    std::vector<node_id> table_nodes;
    std::vector<std::uint32_t> table_row_firsts;
    std::vector<std::uint32_t> row_cell_firsts;
    std::vector<std::string_view> cells;

    std::vector<node_id> list_nodes;
    std::vector<std::uint32_t> list_item_firsts;
    std::vector<std::string_view> list_items;

    [[nodiscard]] std::size_t size() const noexcept { return levels.size(); }
};

/**
 * @brief Fills @p out with the outline of the document @p t was parsed
 * from. @p out is cleared first and keeps its capacity.
 */
void build_outline(std::string_view source,
    const compass::org::types::tables_view& t, outline& out);

/**
 * @brief The first child of @p n whose title matches @p title in any case,
 * or none. @p n may be none, which has no children.
 */
[[nodiscard]] node_id section(const outline& o, node_id n,
    std::string_view title);

/**
 * @brief The children of @p n, in order.
 */
[[nodiscard]] std::vector<node_id> children(const outline& o, node_id n);

/**
 * @brief A value looked up by key: found is false when there is none.
 * property gives property @p key of node @p n.
 */
struct lookup {
    bool found = false;
    std::string_view value;
};
[[nodiscard]] lookup property(const outline& o, node_id n,
    std::string_view key);
[[nodiscard]] lookup frontmatter(const outline& o, std::string_view key);
[[nodiscard]] lookup file_property(const outline& o, std::string_view key);

/**
 * @brief The properties of @p n in the order of their first key, each with
 * its last value.
 */
struct key_value {
    std::string_view key;
    std::string_view value;
};
[[nodiscard]] std::vector<key_value> properties(const outline& o, node_id n);

/**
 * @brief The code of the last source block of @p n named @p name.
 */
[[nodiscard]] lookup named_block(const outline& o, node_id n,
    std::string_view name);

/**
 * @brief The prose of @p n as _strip_body returns it: its lines without the
 * blank ones at either end, with =verbatim= markers removed across lines or,
 * when @p per_line, within each line.
 */
[[nodiscard]] std::string body(const outline& o, node_id n,
    bool per_line = false);

/**
 * @brief Removes the =verbatim= markers of @p text as _strip_org_markup does.
 */
[[nodiscard]] std::string strip_markup(std::string_view text);

/**
 * @brief The tables of @p n, as row and cell ranges: table k of the result
 * holds its header row first.
 */
[[nodiscard]] std::vector<std::uint32_t> tables_of(const outline& o,
    node_id n);
[[nodiscard]] std::uint32_t row_count(const outline& o, std::uint32_t table);
[[nodiscard]] std::uint32_t cell_count(const outline& o, std::uint32_t row);
[[nodiscard]] std::uint32_t first_row(const outline& o, std::uint32_t table);
[[nodiscard]] std::string_view cell(const outline& o, std::uint32_t row,
    std::uint32_t i);

}

#endif

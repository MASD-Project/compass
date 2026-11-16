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
#ifndef COMPASS_ORG_TYPES_ELEMENT_TABLE_HPP
#define COMPASS_ORG_TYPES_ELEMENT_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/source_range.hpp"

namespace compass::org::types {

/**
 * @brief The greater elements and elements of the Org Syntax specification.
 *
 * The names follow org-element. Where org-element tells two variants apart
 * by a property and the variant decides what children a row has, each
 * variant is a kind: table and table_el, table_row and table_rule.
 */
enum class element_kind : std::uint8_t {
    document,
    section,
    heading,
    inlinetask,
    plain_list,
    item,
    table,
    table_el,
    table_row,
    table_rule,
    drawer,
    property_drawer,
    node_property,
    footnote_definition,
    quote_block,
    center_block,
    special_block,
    dynamic_block,
    verse_block,
    src_block,
    example_block,
    export_block,
    comment_block,
    paragraph,
    keyword,
    babel_call,
    planning,
    clock,
    diary_sexp,
    comment,
    fixed_width,
    horizontal_rule,
    latex_environment
};

/**
 * @brief Value of a post_blanks column that stands for 255 or more.
 *
 * The exact count is then read from the source.
 */
inline constexpr std::uint8_t post_blank_saturated = 0xFF;

/**
 * @brief The element tree of a document, one row per element in pre-order.
 *
 * Row 0 is the document. The children of e are e.next(), then
 * subtree_ends[child] for each following sibling, until subtree_ends[e].
 * ranges, contents and post_blanks carry org-element's :begin and :end,
 * :contents-begin and :contents-end, and :post-blank. The objects an element
 * holds directly are the rows of the object table from object_firsts[e] up to
 * object_firsts of the next row, or up to the end of the object table for the
 * last row.
 */
struct element_table {
    column<element_id, element_kind> kinds;
    column<element_id, std::uint8_t> post_blanks;
    column<element_id, source_range> ranges;
    column<element_id, source_range> contents;
    column<element_id, element_id> parents;
    column<element_id, element_id> subtree_ends;
    column<element_id, object_id> object_firsts;

    [[nodiscard]] std::size_t size() const noexcept { return kinds.size(); }
};

inline constexpr std::size_t element_row_bytes = row_bytes(
    &element_table::kinds, &element_table::post_blanks,
    &element_table::ranges, &element_table::contents,
    &element_table::parents, &element_table::subtree_ends,
    &element_table::object_firsts);

static_assert(element_row_bytes == 30);

}

#endif

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
#ifndef COMPASS_ORG_TYPES_HEADING_TABLE_HPP
#define COMPASS_ORG_TYPES_HEADING_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/source_range.hpp"

namespace compass::org::types {

/**
 * @brief Whether the TODO keyword of a heading stands before or after the
 * bar of the document's vocabulary.
 */
enum class todo_type : std::uint8_t { none, todo, done };

/**
 * @brief Whether a heading starts with COMMENT.
 */
enum class comment_state : std::uint8_t { live, commented };

/**
 * @brief Headings and inlinetasks, in document order.
 *
 * priorities holds org's priority value, a character code or a number, and 0
 * for none. titles is the title text without keyword, priority and tags; its
 * objects are the heading element's own objects. The tags of h are the rows
 * of the heading tag table from tag_firsts[h] up to tag_firsts of the next
 * heading, or up to the end of that table for the last heading. Its
 * properties are the rows of the property table from property_firsts[h],
 * likewise. The properties before property_firsts of the first heading
 * belong to the document.
 */
template <template <typename, column_value> class Column>
struct basic_heading_table {
    Column<heading_id, element_id> elements;
    Column<heading_id, std::uint16_t> levels;
    Column<heading_id, name_id> todos;
    Column<heading_id, todo_type> todo_types;
    Column<heading_id, std::uint8_t> priorities;
    Column<heading_id, comment_state> comments;
    Column<heading_id, source_range> titles;
    Column<heading_id, tag_id> tag_firsts;
    Column<heading_id, property_id> property_firsts;

    [[nodiscard]] std::size_t size() const noexcept { return elements.size(); }

    template <typename F, typename... Tables>
    static constexpr void for_each_column(F&& f, Tables&&... t) {
        f(t.elements...); f(t.levels...); f(t.todos...); f(t.todo_types...);
        f(t.priorities...); f(t.comments...); f(t.titles...);
        f(t.tag_firsts...); f(t.property_firsts...);
    }
};

using heading_table = basic_heading_table<column>;
using heading_table_view = basic_heading_table<column_view>;

inline constexpr std::size_t heading_row_bytes = row_bytes(
    &heading_table::elements, &heading_table::levels, &heading_table::todos,
    &heading_table::todo_types, &heading_table::priorities,
    &heading_table::comments, &heading_table::titles,
    &heading_table::tag_firsts, &heading_table::property_firsts);

static_assert(heading_row_bytes == 29);

/**
 * @brief The tags written on headings, one row per tag.
 */
template <template <typename, column_value> class Column>
struct basic_heading_tag_table {
    Column<tag_id, name_id> names;

    [[nodiscard]] std::size_t size() const noexcept { return names.size(); }

    template <typename F, typename... Tables>
    static constexpr void for_each_column(F&& f, Tables&&... t) {
        f(t.names...);
    }
};

using heading_tag_table = basic_heading_tag_table<column>;
using heading_tag_table_view = basic_heading_tag_table<column_view>;

inline constexpr std::size_t heading_tag_row_bytes =
    row_bytes(&heading_tag_table::names);

static_assert(heading_tag_row_bytes == 4);

}

#endif

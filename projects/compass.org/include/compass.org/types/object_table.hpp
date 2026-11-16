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
#ifndef COMPASS_ORG_TYPES_OBJECT_TABLE_HPP
#define COMPASS_ORG_TYPES_OBJECT_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/source_range.hpp"

namespace compass::org::types {

/**
 * @brief The objects of the Org Syntax specification, named as in
 * org-element.
 *
 * Plain text is not an object here: it is the source between the objects of
 * a run.
 */
enum class object_kind : std::uint8_t {
    bold,
    italic,
    underline,
    strike_through,
    code,
    verbatim,
    link,
    timestamp,
    footnote_reference,
    inline_src_block,
    inline_babel_call,
    macro,
    export_snippet,
    latex_fragment,
    entity,
    statistics_cookie,
    radio_target,
    target,
    line_break,
    subscript,
    superscript,
    table_cell,
    citation,
    citation_reference
};

/**
 * @brief The objects of a document, one row per object.
 *
 * The objects of affiliated keywords come first, then the objects of the
 * elements; each group is in pre-order, which is source order. The objects
 * nested in o are the rows from o.next() up to subtree_ends[o]. contents is
 * empty for an object that holds no other object. post_blanks counts the
 * spaces after the object and saturates like the element column.
 */
template <template <typename, column_value> class Column>
struct basic_object_table {
    Column<object_id, object_kind> kinds;
    Column<object_id, std::uint8_t> post_blanks;
    Column<object_id, source_range> ranges;
    Column<object_id, source_range> contents;
    Column<object_id, object_id> subtree_ends;

    [[nodiscard]] std::size_t size() const noexcept { return kinds.size(); }

    template <typename F, typename... Tables>
    static constexpr void for_each_column(F&& f, Tables&&... t) {
        f(t.kinds...); f(t.post_blanks...); f(t.ranges...); f(t.contents...);
        f(t.subtree_ends...);
    }
};

using object_table = basic_object_table<column>;
using object_table_view = basic_object_table<column_view>;

inline constexpr std::size_t object_row_bytes = row_bytes(
    &object_table::kinds, &object_table::post_blanks, &object_table::ranges,
    &object_table::contents, &object_table::subtree_ends);

static_assert(object_row_bytes == 22);

}

#endif

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
#ifndef COMPASS_ORG_TYPES_PROPERTY_TABLE_HPP
#define COMPASS_ORG_TYPES_PROPERTY_TABLE_HPP

#include <cstddef>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/source_range.hpp"

namespace compass::org::types {

/**
 * @brief Node properties, in document order.
 *
 * names holds the key in upper case. The heading table says which heading
 * owns which rows.
 */
template <template <typename, column_value> class Column>
struct basic_property_table {
    Column<property_id, element_id> elements;
    Column<property_id, name_id> names;
    Column<property_id, source_range> values;

    [[nodiscard]] std::size_t size() const noexcept { return elements.size(); }

    template <typename F, typename... Tables>
    static constexpr void for_each_column(F&& f, Tables&&... t) {
        f(t.elements...); f(t.names...); f(t.values...);
    }
};

using property_table = basic_property_table<column>;
using property_table_view = basic_property_table<column_view>;

inline constexpr std::size_t property_row_bytes = row_bytes(
    &property_table::elements, &property_table::names,
    &property_table::values);

static_assert(property_row_bytes == 16);

}

#endif

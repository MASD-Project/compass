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
#ifndef COMPASS_ORG_TYPES_AFFILIATED_TABLE_HPP
#define COMPASS_ORG_TYPES_AFFILIATED_TABLE_HPP

#include <cstddef>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/source_range.hpp"

namespace compass::org::types {

/**
 * @brief Affiliated keywords, such as #+name:, #+caption: and #+attr_html:,
 * sorted by the element they are attached to.
 *
 * names is the name org-element files the keyword under, in lower case:
 * #+tblname: and #+label: are name, #+result: is results and #+headers: is
 * header. Every row is kept, also where org-element keeps the last only.
 * options is the bracketed part of #+caption[short]: and of #+results[hash]:.
 * The objects of the value are the rows of the object table from
 * object_firsts[a] up to object_firsts of the next row, or up to
 * object_firsts of the document element for the last row.
 */
template <template <typename, column_value> class Column>
struct basic_affiliated_table {
    Column<affiliated_id, element_id> owners;
    Column<affiliated_id, name_id> names;
    Column<affiliated_id, source_range> options;
    Column<affiliated_id, source_range> values;
    Column<affiliated_id, object_id> object_firsts;

    [[nodiscard]] std::size_t size() const noexcept { return owners.size(); }

    template <typename F, typename... Tables>
    static constexpr void for_each_column(F&& f, Tables&&... t) {
        f(t.owners...); f(t.names...); f(t.options...); f(t.values...);
        f(t.object_firsts...);
    }
};

using affiliated_table = basic_affiliated_table<column>;
using affiliated_table_view = basic_affiliated_table<column_view>;

inline constexpr std::size_t affiliated_row_bytes = row_bytes(
    &affiliated_table::owners, &affiliated_table::names,
    &affiliated_table::options, &affiliated_table::values,
    &affiliated_table::object_firsts);

static_assert(affiliated_row_bytes == 28);

}

#endif

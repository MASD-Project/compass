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
 * options is the bracketed part of #+caption[short]: and of #+results[hash]:.
 * The objects of the value are the rows of the object table from
 * object_firsts[a] up to object_firsts of the next row, or up to
 * object_firsts of the document element for the last row.
 */
struct affiliated_table {
    column<affiliated_id, element_id> owners;
    column<affiliated_id, name_id> names;
    column<affiliated_id, source_range> options;
    column<affiliated_id, source_range> values;
    column<affiliated_id, object_id> object_firsts;

    [[nodiscard]] std::size_t size() const noexcept { return owners.size(); }
};

inline constexpr std::size_t affiliated_row_bytes = row_bytes(
    &affiliated_table::owners, &affiliated_table::names,
    &affiliated_table::options, &affiliated_table::values,
    &affiliated_table::object_firsts);

static_assert(affiliated_row_bytes == 28);

}

#endif

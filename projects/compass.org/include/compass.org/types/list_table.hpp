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
#ifndef COMPASS_ORG_TYPES_LIST_TABLE_HPP
#define COMPASS_ORG_TYPES_LIST_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/source_range.hpp"

namespace compass::org::types {

/**
 * @brief The three kinds of plain list.
 */
enum class list_type : std::uint8_t { unordered, ordered, descriptive };

/**
 * @brief Plain lists, in document order.
 */
struct list_table {
    column<list_id, element_id> elements;
    column<list_id, list_type> types;

    [[nodiscard]] std::size_t size() const noexcept { return elements.size(); }
};

inline constexpr std::size_t list_row_bytes =
    row_bytes(&list_table::elements, &list_table::types);

static_assert(list_row_bytes == 5);

/**
 * @brief The checkbox of an item: none, [ ], [X] or [-].
 */
enum class checkbox : std::uint8_t { none, off, on, partial };

/**
 * @brief List items, in document order.
 *
 * tags is the term of a descriptive item; its objects are the item element's
 * own objects. The bullet and the counter are read from the source at the
 * start of the item.
 */
struct item_table {
    column<item_id, element_id> elements;
    column<item_id, checkbox> checkboxes;
    column<item_id, source_range> tags;

    [[nodiscard]] std::size_t size() const noexcept { return elements.size(); }
};

inline constexpr std::size_t item_row_bytes = row_bytes(
    &item_table::elements, &item_table::checkboxes, &item_table::tags);

static_assert(item_row_bytes == 13);

}

#endif

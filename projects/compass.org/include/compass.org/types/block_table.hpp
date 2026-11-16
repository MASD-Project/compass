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
#ifndef COMPASS_ORG_TYPES_BLOCK_TABLE_HPP
#define COMPASS_ORG_TYPES_BLOCK_TABLE_HPP

#include <cstddef>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/source_range.hpp"

namespace compass::org::types {

/**
 * @brief Every #+begin_ and #+begin: block, in document order.
 *
 * The element kind tells the blocks apart. names is the language of a source
 * block, the backend of an export block, the type of a special block and the
 * name of a dynamic block; it is none for the rest. parameters is the rest
 * of the opening line, unparsed. bodies is the text between the opening and
 * closing lines.
 */
struct block_table {
    column<block_id, element_id> elements;
    column<block_id, name_id> names;
    column<block_id, source_range> parameters;
    column<block_id, source_range> bodies;

    [[nodiscard]] std::size_t size() const noexcept { return elements.size(); }
};

inline constexpr std::size_t block_row_bytes = row_bytes(
    &block_table::elements, &block_table::names, &block_table::parameters,
    &block_table::bodies);

static_assert(block_row_bytes == 24);

}

#endif

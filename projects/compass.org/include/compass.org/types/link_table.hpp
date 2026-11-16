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
#ifndef COMPASS_ORG_TYPES_LINK_TABLE_HPP
#define COMPASS_ORG_TYPES_LINK_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/source_range.hpp"

namespace compass::org::types {

/**
 * @brief How a link is written: bare, in angle brackets or in square
 * brackets.
 */
enum class link_format : std::uint8_t { plain, angle, bracket };

/**
 * @brief Links, in the order of their rows in the object table.
 *
 * types is org-element's :type: the scheme written before the colon, or
 * file, custom-id, coderef, fuzzy or radio for a link with no scheme. paths
 * is the target without the scheme. The description is the contents of the
 * object row.
 */
struct link_table {
    column<link_id, object_id> objects;
    column<link_id, name_id> types;
    column<link_id, link_format> formats;
    column<link_id, source_range> paths;

    [[nodiscard]] std::size_t size() const noexcept { return objects.size(); }
};

inline constexpr std::size_t link_row_bytes = row_bytes(&link_table::objects,
    &link_table::types, &link_table::formats, &link_table::paths);

static_assert(link_row_bytes == 17);

}

#endif

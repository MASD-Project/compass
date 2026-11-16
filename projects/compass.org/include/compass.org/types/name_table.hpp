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
#ifndef COMPASS_ORG_TYPES_NAME_TABLE_HPP
#define COMPASS_ORG_TYPES_NAME_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"

namespace compass::org::types {

/**
 * @brief The interned strings of a document: keys, tags, TODO keywords,
 * block names and link types.
 *
 * Each distinct name has one row, so two names of a document are equal when
 * their handles are. The bytes of n run from firsts[n] up to firsts of the
 * next row, or up to the end of the name bytes for the last row.
 */
struct name_table {
    column<name_id, std::uint32_t> firsts;

    [[nodiscard]] std::size_t size() const noexcept { return firsts.size(); }
};

inline constexpr std::size_t name_row_bytes = row_bytes(&name_table::firsts);

static_assert(name_row_bytes == 4);

}

#endif

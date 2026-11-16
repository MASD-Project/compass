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
#ifndef COMPASS_STORE_TYPES_BLOB_ID_HPP
#define COMPASS_STORE_TYPES_BLOB_ID_HPP

#include <array>
#include <compare>
#include <cstdint>

namespace compass::store::types {

/**
 * @brief The ID git gives the content of a file: the 20 raw bytes of the
 * SHA-1 of "blob <size>\0<content>".
 *
 * It is the key of the store. Two files with the same bytes have the same
 * ID in every worktree, whatever their path.
 */
struct blob_id {
    std::array<std::uint8_t, 20> bytes{};

    friend constexpr auto operator<=>(const blob_id&,
        const blob_id&) noexcept = default;
};

}

#endif

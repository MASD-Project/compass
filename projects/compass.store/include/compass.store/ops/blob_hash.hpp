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
#ifndef COMPASS_STORE_OPS_BLOB_HASH_HPP
#define COMPASS_STORE_OPS_BLOB_HASH_HPP

#include <string>
#include <string_view>
#include "compass.store/types/blob_id.hpp"

namespace compass::store::ops {

/**
 * @brief The ID `git hash-object` prints for @p content.
 *
 * It equals the ID git's index records for a clean tracked file with these
 * bytes, when the repository uses SHA-1 and no filter rewrites the file on
 * checkout.
 */
[[nodiscard]] types::blob_id blob_id_of(std::string_view content) noexcept;

/**
 * @brief The ID as git prints it: 40 lower-case hexadecimal digits.
 */
[[nodiscard]] std::string to_hex(const types::blob_id& id);

}

#endif

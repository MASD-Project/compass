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
#ifndef COMPASS_STORE_OPS_ACTION_OBJECT_HPP
#define COMPASS_STORE_OPS_ACTION_OBJECT_HPP

#include <optional>
#include <string>
#include <string_view>
#include "compass.store/types/blob_id.hpp"

namespace compass::store::ops {

/**
 * @brief The bytes of the action object named @p key that holds @p body:
 * the magic "CMPSACT\n", the body and the SHA-1 of the key, the magic and
 * the body.
 *
 * The key is in the checksum, so an object copied or renamed under another
 * key does not load.
 */
[[nodiscard]] std::string encode_action(const types::blob_id& key,
    std::string_view body);

/**
 * @brief The body of the action object @p bytes, or nothing when it is
 * truncated, has another magic or its checksum does not match @p key.
 */
[[nodiscard]] std::optional<std::string_view> decode_action(
    const types::blob_id& key, std::string_view bytes);

}

#endif

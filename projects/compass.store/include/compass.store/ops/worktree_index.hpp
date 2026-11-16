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
#ifndef COMPASS_STORE_OPS_WORKTREE_INDEX_HPP
#define COMPASS_STORE_OPS_WORKTREE_INDEX_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include "compass.store/types/worktree_index.hpp"

namespace compass::store::ops {

/**
 * @brief The version of the worktree index file this build writes and
 * reads. A file of any other version is not read.
 */
inline constexpr std::uint32_t worktree_index_version = 1;

/**
 * @brief The bytes of the worktree index file.
 *
 * All integers are little-endian. A 24-byte header holds the magic
 * "CMPSWTI\n", the version, the number of entries and the written time.
 * Each entry follows in path order: its nine stat fields, its 20-byte ID,
 * the length of its path and the path. The SHA-1 of everything before it
 * ends the file.
 *
 * @pre the paths of @p index are sorted and distinct.
 */
[[nodiscard]] std::string encode_worktree_index(
    const types::worktree_index& index);

/**
 * @brief The worktree index held by @p bytes.
 *
 * @throws std::runtime_error naming the defect when the magic, the version
 * or the trailing SHA-1 is not the expected one, an entry runs past the
 * end, or the paths are not sorted and distinct.
 */
[[nodiscard]] types::worktree_index decode_worktree_index(
    std::string_view bytes);

}

#endif

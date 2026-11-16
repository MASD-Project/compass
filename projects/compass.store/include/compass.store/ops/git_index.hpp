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
#ifndef COMPASS_STORE_OPS_GIT_INDEX_HPP
#define COMPASS_STORE_OPS_GIT_INDEX_HPP

#include <cstdint>
#include <string_view>
#include "compass.store/types/git_index.hpp"

namespace compass::store::ops {

/**
 * @brief The entries of a git index file, versions 2, 3 and 4, as
 * gitformat-index(5) defines them.
 *
 * Extensions whose signature starts with an upper-case letter are optional
 * and skipped. Any other extension is one a reader must understand, such
 * as the link of a split index, and rejects the file. The trailing SHA-1
 * must match the bytes before it, unless it is all zeros, which is what git
 * writes under index.skipHash.
 *
 * @throws std::runtime_error naming the defect when @p bytes are not a
 * well-formed index of a supported version.
 */
[[nodiscard]] types::git_index parse_git_index(std::string_view bytes);

/**
 * @brief True when the mode of an index entry is a regular file, executable
 * or not, rather than a symbolic link, a submodule or a sparse directory.
 */
[[nodiscard]] bool is_regular_file(std::uint32_t mode) noexcept;

/**
 * @brief True when a file is unchanged since its @p entry was recorded,
 * judged from stat data alone.
 *
 * Every field of @p entry must equal the one of @p file, nanoseconds
 * included; git on Linux checks a subset of them, so this never trusts an
 * entry git would not. An entry modified in the same second as the record
 * was @p written, or later, is racily clean: the file could have changed
 * after it was hashed without its stat data changing, so it is not trusted.
 */
[[nodiscard]] bool is_unchanged(const types::stat_data& entry,
    const types::stat_data& file, types::index_time written) noexcept;

/**
 * @brief True when git would call a file clean, so the ID of its entry
 * names the file's bytes: the entry is_unchanged and its size is not zero.
 * Git zeroes the size of an entry it could not prove clean when it wrote
 * the index, so such an entry is never trusted.
 */
[[nodiscard]] bool is_clean(const types::stat_data& entry,
    const types::stat_data& file, types::index_time written) noexcept;

}

#endif

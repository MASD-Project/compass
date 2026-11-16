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
#ifndef COMPASS_STORE_IO_GIT_FILES_HPP
#define COMPASS_STORE_IO_GIT_FILES_HPP

#include <filesystem>
#include <optional>
#include "compass.store/types/blob_id.hpp"
#include "compass.store/types/git_index.hpp"

namespace compass::store::io {

/**
 * @brief Reads and parses a git index file, and records its modification
 * time as the time the index was written.
 *
 * The file is opened read-only and no lock is taken, so a running git is
 * never disturbed. Git replaces its index by renaming a new file over it,
 * so the bytes read are one whole index.
 *
 * @throws std::system_error when the file cannot be read, and
 * std::runtime_error when it is not a well-formed index.
 */
[[nodiscard]] types::git_index read_git_index(
    const std::filesystem::path& file);

/**
 * @brief The stat data of @p file as git would record it, read with lstat,
 * or nothing when it is missing or is not a regular file.
 */
[[nodiscard]] std::optional<types::stat_data> stat_of(
    const std::filesystem::path& file);

/**
 * @brief The blob ID of the bytes of @p file, following a symbolic link.
 *
 * @throws std::system_error when the file cannot be read.
 */
[[nodiscard]] types::blob_id hash_file(const std::filesystem::path& file);

}

#endif

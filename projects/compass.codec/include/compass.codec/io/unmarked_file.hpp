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
#ifndef COMPASS_CODEC_IO_UNMARKED_FILE_HPP
#define COMPASS_CODEC_IO_UNMARKED_FILE_HPP

#include <filesystem>
#include <vector>
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::io {

/**
 * @brief The record of the contents known to hold no exception marker:
 * compass/unmarked under the checkout's own @p git_dir, beside the
 * dependency record.
 */
[[nodiscard]] std::filesystem::path unmarked_file_path(
    const std::filesystem::path& git_dir);

/**
 * @brief The content IDs the record at @p path holds, sorted. A missing or
 * damaged file, or one written for another marker, holds none, so every
 * file is read. Never throws for the content of the file.
 */
[[nodiscard]] std::vector<compass::store::types::blob_id> read_unmarked_file(
    const std::filesystem::path& path);

/**
 * @brief Puts @p ids, sorted, at @p path unless the file already holds
 * them, through a temporary file and a rename.
 * @return true when the file was written.
 * @throws std::filesystem::filesystem_error when the directory, the file or
 * the rename fails.
 */
bool write_unmarked_file(const std::filesystem::path& path,
    const std::vector<compass::store::types::blob_id>& ids);

}

#endif

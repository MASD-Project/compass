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
#ifndef COMPASS_CODEC_IO_DEPENDENCY_FILE_HPP
#define COMPASS_CODEC_IO_DEPENDENCY_FILE_HPP

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include "compass.codec/types/dependency_record.hpp"
#include "compass.codec/types/unit_index.hpp"

namespace compass::codec::io {

/**
 * @brief The dependency record of one checkout: compass/deps under its own
 * @p git_dir, beside the worktree index, so each worktree keeps its own.
 */
[[nodiscard]] std::filesystem::path dependency_file_path(
    const std::filesystem::path& git_dir);

/**
 * @brief What reading the dependency record found.
 * problem is empty when the file was read or is missing; otherwise it says
 * why the file was not used, and record is empty, so every unit runs.
 * bytes is the size of the file.
 */
struct loaded_record {
    types::dependency_record record;
    std::string problem;
    std::uint64_t bytes = 0;
};

/**
 * @brief Reads the record at @p path. A missing file is an empty record; a
 * damaged one, or one of another version, is an empty record with a
 * problem. Never throws for the content of the file.
 */
[[nodiscard]] loaded_record read_dependency_file(
    const std::filesystem::path& path);

/**
 * @brief Puts @p record at @p path unless the file already holds exactly
 * these bytes, through a temporary file and a rename, so a reader sees the
 * old record or the new one.
 * @return true when the file was written.
 * @throws std::filesystem::filesystem_error when the directory, the file or
 * the rename fails.
 */
bool write_dependency_file(const std::filesystem::path& path,
    const types::dependency_record& record);

/**
 * @brief What reading the unit index of a dependency record found.
 * problem is empty when the index was read, or the file is missing and
 * missing is true; otherwise it says why the index was not used and
 * index is empty.
 */
struct loaded_index {
    types::unit_index index;
    bool missing = false;
    std::string problem;
};

/**
 * @brief Reads the unit index of the record at @p path: its header, its
 * footer and the index they locate, and none of its units. Never throws
 * for the content of the file.
 */
[[nodiscard]] loaded_index read_unit_index(const std::filesystem::path& path);

/**
 * @brief The record holding @p units of the record at @p path, in the
 * order given, each read alone and checked against the SHA-1 @p index
 * holds for it. The problem is not empty, and the record is, when a unit
 * cannot be read or does not match, as when the file was replaced since
 * the index was read.
 */
[[nodiscard]] loaded_record read_record_units(
    const std::filesystem::path& path, const types::unit_index& index,
    std::span<const types::record_unit_id> units);

}

#endif

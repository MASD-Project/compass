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
#ifndef COMPASS_CODEC_OPS_FORMAT_CACHE_HPP
#define COMPASS_CODEC_OPS_FORMAT_CACHE_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/formatter.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::ops {

/**
 * @brief Bumped by hand when the keys below or what they name change.
 */
inline constexpr std::uint32_t format_cache_version = 1;

/**
 * @brief The formatter of @p formatters that applies to @p path, by the
 * extension of its file name as Python's Path.suffix reads it, the first
 * listed winning; nothing when none does.
 */
[[nodiscard]] std::optional<std::size_t> formatter_of(
    std::span<const types::formatter> formatters, std::string_view path);

/**
 * @brief The key of everything a formatter's output depends on beside its
 * input: its name, its @p version as the program reports it, its arguments
 * before the style path is put in, its passes and the blob ID of its
 * @p style file's bytes. The program's path is not part of it.
 */
[[nodiscard]] compass::store::types::blob_id formatter_key(
    const types::formatter& f, std::string_view version,
    const compass::store::types::blob_id& style);

/**
 * @brief The key of the formatted form of the bytes whose blob ID is
 * @p raw at @p path, formatted by the formatter of key @p formatter.
 *
 * The path is part of the key because a formatter reads it: clang-format
 * sorts a file's main include first by its name, and prettier picks its
 * parser and its overrides by the file name.
 */
[[nodiscard]] compass::store::types::blob_id formatted_key(
    const compass::store::types::blob_id& formatter, std::string_view path,
    const compass::store::types::blob_id& raw);

/**
 * @brief The key of the version a program reported, from what identifies
 * the program file on disk: its resolved @p path, @p size and modification
 * time @p mtime_ns. Replacing the program changes the key.
 */
[[nodiscard]] compass::store::types::blob_id program_key(
    std::string_view path, std::uint64_t size, std::int64_t mtime_ns);

/**
 * @brief The command line of one pass of @p f over @p files: the program,
 * the arguments with types::style_placeholder replaced by the style path,
 * then the files.
 */
[[nodiscard]] std::vector<std::string> format_command(
    const types::formatter& f, std::span<const std::string> files);

}

#endif

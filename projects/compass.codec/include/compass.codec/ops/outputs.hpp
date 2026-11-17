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
#ifndef COMPASS_CODEC_OPS_OUTPUTS_HPP
#define COMPASS_CODEC_OPS_OUTPUTS_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/output_set.hpp"
#include "compass.codec/types/registry.hpp"
#include "compass.codec/types/run_result.hpp"
#include "compass.codec/types/written_outputs.hpp"

namespace compass::codec::ops {

/**
 * @brief The version of the written-outputs file this build writes and
 * reads. A file of any other version is not read.
 */
inline constexpr std::uint32_t written_outputs_version = 1;

/**
 * @brief The files of @p result with their codecs and blob IDs, taken from
 * its record, but for a codec whose files were formatted, whose files and
 * IDs are its formatted ones. A path two parts of a staged codec emit
 * is taken once, from the later part. Every enabled codec of @p r is
 * complete, as a run either calls a unit or reuses its recorded outputs,
 * but for one that refused an output.
 */
[[nodiscard]] types::output_set outputs_of(const types::registry& r,
    const types::run_result& result);

/**
 * @brief True when codec @p codec of @p r emits files and reported a
 * refusal in @p result. Such a codec is not complete, so what it wrote
 * before stays on disk.
 */
[[nodiscard]] bool refused(const types::registry& r,
    const types::run_result& result, std::size_t codec);

/**
 * @brief True when @p path names a file under an output root and nothing
 * else: relative, not empty, and with no empty, "." or ".." component.
 */
[[nodiscard]] bool is_plain_relative(std::string_view path) noexcept;

/**
 * @brief Why @p outputs cannot be written, one line per defect: a path
 * that is not plain relative, or a path two rows produce, naming both
 * codecs. Empty when every path is plain and distinct.
 */
[[nodiscard]] std::vector<std::string> check_outputs(
    const types::output_set& outputs);

/**
 * @brief The bytes of the written-outputs file.
 * All integers are little-endian. The magic "CMPSOUT\n", the version, the
 * entry count, the written time and the root come first. Each entry
 * follows in path order: path, codec, blob ID and the nine stat fields. A
 * string is its length and its bytes. The SHA-1 of everything before it
 * ends the file.
 * @pre the paths of @p record are sorted, distinct and plain relative.
 */
[[nodiscard]] std::string encode_written_outputs(
    const types::written_outputs& record);

/**
 * @brief The record held by @p bytes.
 * @throws std::runtime_error naming the defect when the magic, the version
 * or the trailing SHA-1 is not the expected one, a field runs past the
 * end, or the paths are not sorted, distinct and plain relative.
 */
[[nodiscard]] types::written_outputs decode_written_outputs(
    std::string_view bytes);

}

#endif

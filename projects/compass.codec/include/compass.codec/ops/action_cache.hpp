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
#ifndef COMPASS_CODEC_OPS_ACTION_CACHE_HPP
#define COMPASS_CODEC_OPS_ACTION_CACHE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include "compass.codec/types/action_manifest.hpp"
#include "compass.codec/types/dependency_record.hpp"
#include "compass.codec/types/file_table.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::ops {

/**
 * @brief The version of the manifest and result formats. It is part of
 * every key, so a build that changes a format finds none of the objects of
 * another.
 */
inline constexpr std::uint32_t action_format_version = 1;

/**
 * @brief How many input sets a manifest keeps. A new set goes first and
 * the oldest falls off, so a document that changes back and forth between
 * a few states finds each of them.
 */
inline constexpr std::size_t manifest_capacity = 8;

/**
 * @brief The key of the manifest of a unit: what is known before it runs.
 * That is the codec's name and version, its configuration hash and the
 * unit's identity: for a document unit the path of its document and its
 * content ID, for a corpus unit an empty path and its type set hash.
 */
[[nodiscard]] compass::store::types::blob_id manifest_key(
    std::string_view codec, std::uint32_t version,
    const compass::store::types::blob_id& configuration,
    std::string_view document, const compass::store::types::blob_id& subject);

/**
 * @brief The key of the result of unit @p u of @p record: its manifest key
 * and its inputs, sorted by kind and key, each with whether it was found
 * and its value.
 */
[[nodiscard]] compass::store::types::blob_id result_key(
    const compass::store::types::blob_id& manifest,
    const types::dependency_record& record, types::record_unit_id u);

/**
 * @brief Puts the inputs of unit @p u of @p record and its result key first
 * in @p m, dropping an earlier copy of the same set and any set past
 * manifest_capacity. False when the set was already first, so the manifest
 * need not be written.
 */
bool add_input_set(types::action_manifest& m,
    const types::dependency_record& record, types::record_unit_id u);

/**
 * @brief The body of a manifest object: the format version, the set count,
 * then per set its result key and its inputs (kind, key, found, value).
 */
[[nodiscard]] std::string encode_manifest(const types::action_manifest& m);

/**
 * @brief The manifest held by @p body, or nothing when it is not one this
 * build writes.
 */
[[nodiscard]] std::optional<types::action_manifest> decode_manifest(
    std::string_view body);

/**
 * @brief What a cached unit produced: its files, and its findings encoded
 * as encode_diagnostics encodes them, which point into the body decoded.
 */
struct action_result {
    types::file_table files;
    std::string_view diagnostics;
};

/**
 * @brief The body of the result object of unit @p u of @p record: the
 * format version, the files (path, content) and the encoded findings.
 */
[[nodiscard]] std::string encode_result(
    const types::dependency_record& record, types::record_unit_id u);

/**
 * @brief The result held by @p body, or nothing when it is not one this
 * build writes.
 */
[[nodiscard]] std::optional<action_result> decode_result(
    std::string_view body);

}

#endif

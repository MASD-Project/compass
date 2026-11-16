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
#ifndef COMPASS_CODEC_OPS_DEPENDENCIES_HPP
#define COMPASS_CODEC_OPS_DEPENDENCIES_HPP

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include "compass.codec/types/corpus.hpp"
#include "compass.codec/types/corpus_index.hpp"
#include "compass.codec/types/dependency_record.hpp"
#include "compass.codec/types/diagnostic_table.hpp"
#include "compass.codec/types/registry.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/type_registry.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::ops {

/**
 * @brief The version of the dependency record file this build writes and
 * reads. A file of any other version is not read, so every unit runs.
 */
inline constexpr std::uint32_t dependency_record_version = 3;

/**
 * @brief The row of every document by path and by ID property.
 */
[[nodiscard]] types::corpus_index index_corpus(const types::corpus& c);

/**
 * @brief The SHA-1 of the option keys and values of @p codec, in declared
 * order. A unit recorded under another value runs again.
 */
[[nodiscard]] compass::store::types::blob_id configuration_hash(
    const types::registry& r, types::codec_id codec);

/**
 * @brief The SHA-1 of the tables of the type registry.
 */
[[nodiscard]] compass::store::types::blob_id registry_hash(
    const compass::org::types::type_registry& types);

/**
 * @brief The SHA-1 of the path and content ID of each of @p rows, in byte
 * order of the paths: the value of a type_set input.
 */
[[nodiscard]] compass::store::types::blob_id type_set_hash(
    const types::corpus& c,
    std::span<const compass::org::types::document_id> rows);

/**
 * @brief The findings [@p begin, @p end) of @p t as the record keeps them:
 * per finding, the path of its document, its range, rule, severity,
 * message and fix.
 */
[[nodiscard]] std::string encode_diagnostics(const types::corpus& c,
    const types::diagnostic_table& t, std::uint32_t begin,
    std::uint32_t end);

/**
 * @brief Appends the findings of @p bytes to @p out, naming each document
 * by its row in the corpus of @p index; with no @p out, only checks that
 * they would decode.
 * @return false when a path names no document or the bytes are not what
 * encode_diagnostics writes; @p out then holds a prefix of the findings.
 */
[[nodiscard]] bool decode_diagnostics(std::string_view bytes,
    const types::corpus_index& index, types::diagnostic_table* out);

/**
 * @brief The bytes of the dependency record file.
 * All integers are little-endian. The magic "CMPSDEP\n", the version and
 * the unit count come first. Each unit follows: codec name, version,
 * configuration hash, document path, its inputs (kind, key, found, value),
 * its files (path, blob ID, content), its findings (count, hash, encoded
 * bytes) and the keys of its manifest and result objects. A string is its length and its bytes. The SHA-1 of
 * everything before it ends the file.
 */
[[nodiscard]] std::string encode_dependency_record(
    const types::dependency_record& record);

/**
 * @brief The dependency record held by @p bytes.
 * @throws std::runtime_error naming the defect when the magic, the version
 * or the trailing SHA-1 is not the expected one, an input has an unknown
 * kind, or a field runs past the end.
 */
[[nodiscard]] types::dependency_record decode_dependency_record(
    std::string_view bytes);

}

#endif

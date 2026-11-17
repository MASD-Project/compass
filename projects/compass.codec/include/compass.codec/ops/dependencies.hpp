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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus.hpp"
#include "compass.codec/types/corpus_index.hpp"
#include "compass.codec/types/dependency_record.hpp"
#include "compass.codec/types/diagnostic_table.hpp"
#include "compass.codec/types/registry.hpp"
#include "compass.codec/types/unit_index.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/type_registry.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::ops {

/**
 * @brief The version of the dependency record file this build writes and
 * reads. A file of any other version is not read, so every unit runs.
 */
inline constexpr std::uint32_t dependency_record_version = 5;

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
 * per finding, whether it names a file rather than a document, the path
 * of its document or file, its range, rule, severity, message and fix.
 */
[[nodiscard]] std::string encode_diagnostics(const types::corpus& c,
    const types::diagnostic_table& t, std::uint32_t begin,
    std::uint32_t end);

/**
 * @brief Appends the findings of @p bytes to @p out, naming each document
 * by its row in the corpus of @p index; with no @p out, only checks that
 * they would decode.
 * @return false when a document's path names no document of the corpus, or
 * the bytes are not what
 * encode_diagnostics writes; @p out then holds a prefix of the findings.
 */
[[nodiscard]] bool decode_diagnostics(std::string_view bytes,
    const types::corpus_index& index, types::diagnostic_table* out);

/**
 * @brief The paths the findings of @p bytes name, as encode_diagnostics
 * writes them, in order and with repeats; none when the bytes do not
 * decode.
 */
[[nodiscard]] std::vector<std::string_view> diagnostic_paths(
    std::string_view bytes);

/**
 * @brief The bytes of the dependency record file.
 * All integers are little-endian and a string is its length and its
 * bytes. A 16-byte header comes first: the magic "CMPSDEP\n", the version
 * and the unit count. The heads of the units follow, end to end, each
 * with its codec name, version, configuration hash, document path, inputs
 * (kind, key, found, value), files (path, blob ID, size), findings
 * (count, hash, encoded bytes) and the keys of its manifest and result
 * objects. Then the contents of the files, unit by unit and file by file.
 * Then the unit index: the offset of each head and of the end of the
 * last, the SHA-1 of each head, the offset of each unit's contents and of
 * their end, and the subjects, each a kind, a key and the units it
 * names. A 28-byte footer ends the file: the offset of the index and the
 * SHA-1 of the header followed by the index. So the index, a unit's head
 * and any one file can be read and checked without the rest: a head by
 * its SHA-1, a file by its blob ID.
 */
[[nodiscard]] std::string encode_dependency_record(
    const types::dependency_record& record);

/**
 * @brief The dependency record held by @p bytes, every head checked
 * against its SHA-1 and every file against its blob ID.
 * @throws std::runtime_error naming the defect when the magic, the version,
 * the index's SHA-1, a head's or a file's is not the expected one, an
 * input or a subject has an unknown kind, or a field runs past its end.
 */
[[nodiscard]] types::dependency_record decode_dependency_record(
    std::string_view bytes);

/**
 * @brief Where the unit index of a dependency record file is: the offset
 * and size of its bytes, and the SHA-1 they and the header must have.
 */
struct record_layout {
    std::uint64_t index_offset = 0;
    std::uint64_t index_size = 0;
    compass::store::types::blob_id digest;
};

/**
 * @brief The size of the header and of the footer of a dependency record
 * file, which locate_unit_index reads.
 */
inline constexpr std::size_t record_header_size = 16;
inline constexpr std::size_t record_footer_size = 28;

/**
 * @brief The layout of a dependency record file of @p file_size bytes
 * whose first record_header_size bytes are @p header and last
 * record_footer_size bytes are @p footer.
 * @throws std::runtime_error when the magic or version is not the expected
 * one or the index lies outside the file.
 */
[[nodiscard]] record_layout locate_unit_index(std::string_view header,
    std::string_view footer, std::uint64_t file_size);

/**
 * @brief The unit index whose bytes are @p section, of a file whose
 * header is @p header, laid out as @p layout says.
 * @throws std::runtime_error when the SHA-1 of the header and the section
 * is not the digest of @p layout, or the section does not decode.
 */
[[nodiscard]] types::unit_index decode_unit_index(std::string_view header,
    std::string_view section, const record_layout& layout);

/**
 * @brief Appends the unit whose head is @p head to @p to, with the
 * contents of its files taken from @p contents, the unit's contents, or
 * left empty without them, and returns the size of each of its files.
 * @throws std::runtime_error when the SHA-1 of @p head is not @p hash,
 * the bytes are not one head, or a file of @p contents does not have its
 * blob ID.
 */
std::vector<std::uint64_t> decode_record_unit(std::string_view head,
    const compass::store::types::blob_id& hash,
    types::dependency_record& to,
    std::optional<std::string_view> contents = std::nullopt);

/**
 * @brief The units @p index links to the subject of kind @p kind and key
 * @p key, in ascending order; none when there is no such subject.
 */
[[nodiscard]] std::span<const types::record_unit_id> units_of(
    const types::unit_index& index, types::subject_kind kind,
    std::string_view key);

/**
 * @brief A file of the checkout as an edit leaves it: its path, relative
 * to the checkout, and, when it lies under the output root, relative to
 * that root; whether it is a document, whether the edit may have created
 * it, and the ID properties it holds.
 */
struct edited_file {
    std::string_view path;
    std::optional<std::string_view> output;
    bool document = false;
    bool created = true;
    std::span<const std::string_view> ids;
};

/**
 * @brief The units of @p index whose result an edit to @p file may change,
 * or that report on it: those that read it, produced it or have a finding
 * on it, or, when it may be new, list a pattern it matches; and, for a
 * document, those that
 * depend on the whole set of documents and those that looked up one of
 * its IDs. Ascending, each once. A unit that read the type registry is
 * not among them unless otherwise named: an edit to a contract page
 * changes what every unit sees.
 */
[[nodiscard]] std::vector<types::record_unit_id> units_touching(
    const types::unit_index& index, const edited_file& file);

}

#endif

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
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.codec/types/action_manifest.hpp"
#include "compass.codec/types/dependency_record.hpp"
#include "compass.codec/types/descriptor.hpp"
#include "compass.codec/types/file_table.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::ops {

/**
 * @brief The version of the manifest and result formats. It is part of
 * every key, so a build that changes a format finds none of the objects of
 * another.
 */
inline constexpr std::uint32_t action_format_version = 5;

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
 * content ID, for a corpus unit an empty path and an all-zero ID, since
 * what a corpus unit read is known only from its input sets.
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
 * @brief What a cached unit produced: its files, its findings encoded as
 * encode_diagnostics encodes them and, for a scan, its scan table as
 * encode_scan writes it, all pointing into the body decoded.
 */
struct action_result {
    types::file_table files;
    std::string_view diagnostics;
    std::string_view scan;
};

/**
 * @brief The body of the result object of unit @p u of @p record: the
 * format version, the files (path, content), the encoded findings and
 * @p scan, the unit's scan table, empty for a unit that is not a scan.
 */
[[nodiscard]] std::string encode_result(
    const types::dependency_record& record, types::record_unit_id u,
    std::string_view scan = {});

/**
 * @brief The result held by @p body, or nothing when it is not one this
 * build writes.
 */
[[nodiscard]] std::optional<action_result> decode_result(
    std::string_view body);

/**
 * @brief The key of the fact object of the file at @p path, holding
 * @p content, as the map step of the scan @p codec, at @p version and with
 * the configuration hash @p configuration, makes it: what the fact and the
 * findings of one file depend on. The path is part of it because the
 * findings name their file.
 */
[[nodiscard]] compass::store::types::blob_id scan_fact_key(
    std::string_view codec, std::uint32_t version,
    const compass::store::types::blob_id& configuration,
    std::string_view path, const compass::store::types::blob_id& content);

/**
 * @brief What the map step of a scan made of one file: its fact and its
 * findings, encoded as encode_diagnostics encodes them.
 */
struct scan_fact {
    std::string_view fact;
    std::string_view findings;
};

/**
 * @brief The body of the fact object of @p f.
 */
[[nodiscard]] std::string encode_scan_fact(const scan_fact& f);

/**
 * @brief The fact object encode_scan_fact wrote as @p bytes, pointing into
 * them, or nothing when they are not one.
 */
[[nodiscard]] std::optional<scan_fact> decode_scan_fact(
    std::string_view bytes);

/**
 * @brief The digest of what a reduce step is given: each file's path and
 * fact, in the order @p files lists them.
 */
[[nodiscard]] compass::store::types::blob_id scan_digest(
    std::span<const types::scanned_file> files);

/**
 * @brief What a scan's result keeps of its call, so that the next call
 * may report its reduce step's findings without reducing: how many files
 * it reduced, the digest of their paths and facts, the paths the reduce
 * step read itself, and its findings, encoded as encode_diagnostics
 * encodes them. Each file's fact and findings are in its fact object, not
 * here, so the table's size does not grow with the files.
 */
struct scan_table {
    std::uint32_t files = 0;
    compass::store::types::blob_id digest;
    std::vector<std::string_view> read_by_reduce;
    std::string_view reduce_findings;
};

/**
 * @brief The bytes of @p t.
 */
[[nodiscard]] std::string encode_scan(const scan_table& t);

/**
 * @brief The scan table encode_scan wrote as @p bytes, pointing into
 * them, or nothing when they are not one.
 */
[[nodiscard]] std::optional<scan_table> decode_scan(std::string_view bytes);

/**
 * @brief The bytes of @p s, which the result object of a staged codec's
 * prepare step keeps where a scan keeps its table, so that a run whose
 * record stands in for prepare knows the parts and the facts without
 * calling it.
 */
[[nodiscard]] std::string encode_stage(const types::stage& s);

/**
 * @brief The stage encode_stage wrote as @p bytes, or nothing when they
 * are not one.
 */
[[nodiscard]] std::optional<types::stage> decode_stage(
    std::string_view bytes);

}

#endif

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
#ifndef COMPASS_ORG_IO_OBJECT_HPP
#define COMPASS_ORG_IO_OBJECT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>
#include "compass.org/types/tables.hpp"

namespace compass::org::io {

/**
 * @brief The layout of an object: the header and the set, order and value
 * types of the columns.
 *
 * Bumped by hand whenever any of them changes. An object of another version
 * does not decode, so it is parsed again.
 */
inline constexpr std::uint32_t object_format_version = 1;

/**
 * @brief Every column starts at a multiple of this many bytes from the start
 * of the object, which is at least the alignment of any column value.
 */
inline constexpr std::size_t object_alignment = 8;

inline constexpr std::array<char, 8> object_magic{
    'C', 'M', 'P', 'S', 'O', 'R', 'G', '\n'};

/**
 * @brief What the writer stores in the byte order field. A reader of the
 * other byte order sees 0x04030201 and rejects the object.
 */
inline constexpr std::uint32_t object_byte_order = 0x01020304;

/**
 * @brief What an object is the parse of: the git blob ID of the source, as
 * its 20 raw bytes, and the version of the parser that read it.
 */
struct object_key {
    std::array<std::uint8_t, 20> blob_id{};
    std::uint32_t parser_version = 0;
};

/**
 * @brief The first bytes of an object, little-endian, with no padding.
 *
 * Offsets count bytes from the start of the object, so an object means the
 * same wherever it is mapped. rows has one entry per table and offsets one
 * per column, both in the order of for_each_table and for_each_column; the
 * columns of a table all have its row count. The checksum covers every byte
 * after the checksum field: the rest of the header, the columns and the
 * padding between them.
 */
struct object_header {
    std::array<char, 8> magic;
    std::uint32_t byte_order;
    std::uint32_t format_version;
    std::uint32_t parser_version;
    std::array<std::uint8_t, 20> blob_id;
    std::uint64_t size;
    std::uint64_t checksum;
    std::uint64_t name_bytes_offset;
    std::uint64_t name_bytes_size;
    std::array<std::uint64_t, types::table_count> rows;
    std::array<std::uint64_t, types::column_count> offsets;
};

static_assert(std::has_unique_object_representations_v<object_header>);
static_assert(sizeof(object_header) % object_alignment == 0);

/**
 * @brief Why bytes are not an object this build can read.
 *
 * Every value means the same to a store: the object is missing, and the
 * source is parsed again.
 */
enum class object_error : std::uint8_t {
    truncated,
    misaligned_buffer,
    bad_magic,
    foreign_byte_order,
    format_version_mismatch,
    parser_version_mismatch,
    blob_id_mismatch,
    size_mismatch,
    column_misaligned,
    column_out_of_bounds
};

[[nodiscard]] std::string_view to_string(object_error e) noexcept;

/**
 * @brief Writes one document as an object into @p out, replacing what it
 * held.
 *
 * The object is the header, then each column as its raw array, then the
 * name bytes, each padded with zeros to object_alignment. The same tables
 * and key always give the same bytes.
 */
void encode(const types::tables_view& tables, const object_key& key,
    std::vector<std::byte>& out);

/**
 * @brief The tables of an object, as spans into @p object.
 *
 * Nothing is copied or converted: decoding checks the header and points a
 * span at each column. It accepts the bytes only when the magic, byte
 * order, format version, parser version and blob ID are the expected ones,
 * the recorded size is the size of @p object, and every column starts
 * aligned and lies inside @p object. So no span reaches outside the bytes,
 * whatever they hold. @p object must start at an address that is a multiple
 * of object_alignment, as a mapped file does.
 *
 * The values inside the columns are not checked: see checksum_matches.
 */
[[nodiscard]] std::expected<types::tables_view, object_error> decode(
    std::span<const std::byte> object, const object_key& key) noexcept;

/**
 * @brief True when the bytes after the checksum field hash to the checksum
 * the header records.
 *
 * It reads every byte of the object. A handle inside a column is only as
 * good as the bytes it was read from, so a reader that cannot trust the
 * file system checks this before it follows one. The checksum finds torn
 * writes and decayed bytes; it is not cryptographic. @p object must be at
 * least a header long, which decode has checked.
 */
[[nodiscard]] bool checksum_matches(
    std::span<const std::byte> object) noexcept;

}

#endif

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
#ifndef COMPASS_STORE_OPS_PACK_HPP
#define COMPASS_STORE_OPS_PACK_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include "compass.store/types/pack.hpp"

namespace compass::store::ops {

/**
 * @brief The layout of the store: 1 was a file per object, manifest and
 * result; 2 is packs. compass --version prints it.
 */
inline constexpr std::uint32_t store_layout_version = 2;

/**
 * @brief The version of the pack file format.
 */
inline constexpr std::uint32_t pack_version = 1;

/**
 * @brief Every entry starts at a multiple of this offset, so a mapped
 * entry is as aligned as a mapped file of its own.
 */
inline constexpr std::size_t pack_alignment = 8;

inline constexpr std::size_t pack_header_size = 16;

/**
 * @brief The 16 bytes a pack starts with: the magic "CMPSPAK\n", the
 * version and four zero bytes.
 */
[[nodiscard]] std::string encode_pack_header();

/**
 * @brief The zero bytes that take @p offset to the next multiple of
 * pack_alignment.
 */
[[nodiscard]] constexpr std::uint64_t padding_for(
    std::uint64_t offset) noexcept {
    return (pack_alignment - offset % pack_alignment) % pack_alignment;
}

/**
 * @brief The bytes that end a pack whose entries end, padded, at
 * @p index_offset: a fan-out table of 256 counts by the first byte of the
 * key, one 48-byte row per entry in order of key then kind, and a 32-byte
 * footer holding the offset of the index, the entry count and the magic
 * "CMPSEND\n". All integers are little-endian.
 *
 * Of two entries of one kind and key, the one written later is kept, or
 * the later in @p entries when both were written at once.
 */
[[nodiscard]] std::string encode_pack_index(
    std::span<const types::pack_entry> entries, std::uint64_t index_offset);

/**
 * @brief A pack whose index parse_pack has checked.
 */
struct pack_view {
    std::span<const std::byte> file;
    std::uint64_t index_offset = 0;
    std::uint64_t count = 0;
};

/**
 * @brief @p file as a pack, or why it is not one: a wrong magic or
 * version, a size that does not match its footer, a fan-out table that
 * does not count the rows, rows out of order or of an unknown kind, or an
 * entry that is misaligned or reaches past the entries. The bytes of the
 * entries are not read.
 */
[[nodiscard]] std::expected<pack_view, std::string> parse_pack(
    std::span<const std::byte> file);

/**
 * @brief Row @p row of the index of @p p.
 */
[[nodiscard]] types::pack_entry entry_at(const pack_view& p,
    std::uint64_t row) noexcept;

/**
 * @brief The entry of @p p with @p kind and @p key, found through the
 * fan-out table and a binary search, or nothing.
 */
[[nodiscard]] std::optional<types::pack_entry> find_entry(const pack_view& p,
    types::entry_kind kind, const types::blob_id& key) noexcept;

/**
 * @brief The bytes of @p e in @p p.
 */
[[nodiscard]] inline std::span<const std::byte> bytes_of(const pack_view& p,
    const types::pack_entry& e) noexcept {
    return p.file.subspan(e.offset, e.size);
}

/**
 * @brief How many of the smallest packs, of sizes @p ascending in
 * ascending order, a geometric repack merges into one: the fewest such
 * that every pack left is at least twice the size of all the packs
 * smaller than it together, merged ones included. Zero or at least two.
 *
 * Repacking after every write so keeps the pack count logarithmic in the
 * size of the store, and each byte is copied a logarithmic number of
 * times.
 */
[[nodiscard]] std::size_t packs_to_merge(
    std::span<const std::uint64_t> ascending) noexcept;

}

#endif

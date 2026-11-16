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
#ifndef COMPASS_STORE_OPS_GIT_OBJECT_HPP
#define COMPASS_STORE_OPS_GIT_OBJECT_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.store/types/git_object.hpp"

/**
 * The formats of git's object database, read from bytes
 * (gitformat-pack(5) and git's loose object layout). Every function here
 * is pure: the caller maps or reads the files. A function that rejects
 * its input says why in the error and never reads outside the bytes it
 * was given.
 */
namespace compass::store::ops {

/**
 * @brief The ID @p hex names: 40 hexadecimal digits of either case, or
 * nothing.
 */
[[nodiscard]] std::optional<types::object_id> from_hex(
    std::string_view hex) noexcept;

/**
 * @brief The bytes of the zlib stream at the start of @p in, which must
 * inflate to exactly @p size bytes. Bytes after the end of the stream are
 * ignored, as a pack file's next entry follows its stream.
 */
[[nodiscard]] std::expected<std::string, std::string> inflate(
    std::span<const std::byte> in, std::size_t size);

/**
 * @brief The object a loose object file holds: @p file is the zlib stream
 * of "<type> <size>\0<bytes>".
 */
[[nodiscard]] std::expected<types::git_object, std::string>
parse_loose_object(std::span<const std::byte> file);

/**
 * @brief The bytes @p delta builds from @p base: a git delta, which names
 * the size of its base and of its result, then copies ranges of the base
 * and inserts literal bytes.
 */
[[nodiscard]] std::expected<std::string, std::string> apply_delta(
    std::string_view base, std::string_view delta);

/**
 * @brief The entries of a tree object, in the order the tree holds them.
 * Each name views @p tree.
 */
[[nodiscard]] std::expected<std::vector<types::tree_entry>, std::string>
parse_tree(std::string_view tree);

/**
 * @brief The ID of the root tree a commit object names on its first line.
 */
[[nodiscard]] std::expected<types::object_id, std::string> tree_of_commit(
    std::string_view commit);

/**
 * @brief The index of a pack file, version 2, as views into its bytes.
 * ids holds count sorted 20-byte IDs. offsets holds one big-endian word
 * per ID: the offset of its entry in the pack, or, with the high bit set,
 * the row of large_offsets that holds it as a big-endian 64-bit word.
 */
struct pack_index {
    std::span<const std::byte> fanout;
    std::span<const std::byte> ids;
    std::span<const std::byte> offsets;
    std::span<const std::byte> large_offsets;
    std::uint32_t count = 0;
};

/**
 * @brief The index @p bytes holds. Version 1 and any other version are
 * rejected, and so is an index whose fan-out table is not ascending or
 * whose tables do not fit in the bytes.
 */
[[nodiscard]] std::expected<pack_index, std::string> parse_pack_index(
    std::span<const std::byte> bytes);

/**
 * @brief The offset in the pack of the entry of @p id, or nothing when
 * the pack does not hold it.
 */
[[nodiscard]] std::optional<std::uint64_t> find_offset(
    const pack_index& index, const types::object_id& id) noexcept;

/**
 * @brief The kinds of a pack entry: a whole object of one of the four
 * types, or a delta against a base named by its offset in the same pack
 * or by its ID.
 */
enum class pack_entry_kind : std::uint8_t {
    commit = 1, tree = 2, blob = 3, tag = 4, offset_delta = 6, ref_delta = 7
};

/**
 * @brief The header of one pack entry. size is the size the entry's zlib
 * stream inflates to, which starts at data. base_offset is set for an
 * offset delta and base_id for a reference delta.
 */
struct pack_entry {
    pack_entry_kind kind = pack_entry_kind::blob;
    std::uint64_t size = 0;
    std::uint64_t data = 0;
    std::uint64_t base_offset = 0;
    types::object_id base_id;
};

/**
 * @brief The header of the entry at @p offset of the pack @p pack.
 */
[[nodiscard]] std::expected<pack_entry, std::string> parse_pack_entry(
    std::span<const std::byte> pack, std::uint64_t offset);

}

#endif

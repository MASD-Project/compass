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
#include "compass.org/io/object.hpp"

#include <algorithm>
#include <bit>
#include <cstring>

namespace compass::org::io {

namespace {

using namespace types;

static_assert(std::endian::native == std::endian::little,
    "objects are little-endian and columns are mapped as they are stored");

constexpr std::size_t value_bytes = [] {
    std::size_t n = 0;
    for_each_table([&](const auto& table) {
        for_each_column([&](const auto& column) {
            n += sizeof(*column.values.data());
        }, table);
    }, tables_view{});
    return n;
}();

static_assert(table_count == 13 && column_count == 61 && value_bytes == 268,
    "the columns changed, so the object layout did: bump "
    "object_format_version, then update these counts");

constexpr std::size_t checksum_first = offsetof(object_header, checksum) +
    sizeof(object_header::checksum);

/**
 * @brief Folds the bytes eight at a time, then the tail and the length.
 */
std::uint64_t checksum_of(std::span<const std::byte> bytes) noexcept {
    constexpr std::uint64_t prime = 0x9E3779B97F4A7C15;
    std::uint64_t h = 0xCBF29CE484222325;
    const auto fold = [&](std::uint64_t word) {
        h = std::rotl((h ^ word) * prime, 31);
    };
    std::size_t i = 0;
    for (; i + 8 <= bytes.size(); i += 8) {
        std::uint64_t word;
        std::memcpy(&word, bytes.data() + i, 8);
        fold(word);
    }
    std::uint64_t tail = 0;
    std::memcpy(&tail, bytes.data() + i, bytes.size() - i);
    fold(tail);
    fold(bytes.size());
    return h ^ (h >> 32);
}

std::uint64_t append(std::vector<std::byte>& out,
    std::span<const std::byte> bytes) {
    out.resize((out.size() + object_alignment - 1) / object_alignment *
        object_alignment);
    const auto offset = out.size();
    out.insert(out.end(), bytes.begin(), bytes.end());
    return offset;
}

/**
 * @brief True when @p count values of @p value_size bytes at @p offset start
 * aligned, after the header, and end inside an object of @p size bytes.
 */
std::expected<void, object_error> check(std::uint64_t offset,
    std::uint64_t count, std::size_t value_size, std::uint64_t size) noexcept {
    if (offset % object_alignment != 0)
        return std::unexpected(object_error::column_misaligned);
    if (offset < sizeof(object_header) || offset > size ||
        count > (size - offset) / value_size)
        return std::unexpected(object_error::column_out_of_bounds);
    return {};
}

}

std::string_view to_string(object_error e) noexcept {
    switch (e) {
    case object_error::truncated: return "truncated";
    case object_error::misaligned_buffer: return "misaligned buffer";
    case object_error::bad_magic: return "bad magic";
    case object_error::foreign_byte_order: return "foreign byte order";
    case object_error::format_version_mismatch:
        return "format version mismatch";
    case object_error::parser_version_mismatch:
        return "parser version mismatch";
    case object_error::blob_id_mismatch: return "blob ID mismatch";
    case object_error::size_mismatch: return "size mismatch";
    case object_error::column_misaligned: return "column misaligned";
    case object_error::column_out_of_bounds: return "column out of bounds";
    }
    return "unknown";
}

void encode(const tables_view& tables, const object_key& key,
    std::vector<std::byte>& out) {
    object_header h{};
    h.magic = object_magic;
    h.byte_order = object_byte_order;
    h.format_version = object_format_version;
    h.parser_version = key.parser_version;
    h.blob_id = key.blob_id;

    out.assign(sizeof(object_header), std::byte{0});
    std::size_t table = 0;
    std::size_t column = 0;
    for_each_table([&](const auto& t) {
        h.rows[table++] = t.size();
        for_each_column([&](const auto& c) {
            h.offsets[column++] = append(out, std::as_bytes(c.values));
        }, t);
    }, tables);
    h.name_bytes_offset = append(out, std::as_bytes(tables.name_bytes));
    h.name_bytes_size = tables.name_bytes.size();
    h.size = out.size();

    std::memcpy(out.data(), &h, sizeof(h));
    h.checksum = checksum_of(std::span(out).subspan(checksum_first));
    std::memcpy(out.data(), &h, sizeof(h));
}

std::expected<tables_view, object_error> decode(
    std::span<const std::byte> object, const object_key& key) noexcept {
    if (object.size() < sizeof(object_header))
        return std::unexpected(object_error::truncated);
    if (reinterpret_cast<std::uintptr_t>(object.data()) % object_alignment !=
        0)
        return std::unexpected(object_error::misaligned_buffer);
    object_header h;
    std::memcpy(&h, object.data(), sizeof(h));
    if (h.magic != object_magic)
        return std::unexpected(object_error::bad_magic);
    if (h.byte_order != object_byte_order)
        return std::unexpected(object_error::foreign_byte_order);
    if (h.format_version != object_format_version)
        return std::unexpected(object_error::format_version_mismatch);
    if (h.parser_version != key.parser_version)
        return std::unexpected(object_error::parser_version_mismatch);
    if (h.blob_id != key.blob_id)
        return std::unexpected(object_error::blob_id_mismatch);
    if (h.size != object.size())
        return std::unexpected(object_error::size_mismatch);

    tables_view v;
    std::expected<void, object_error> ok;
    std::size_t table = 0;
    std::size_t column = 0;
    for_each_table([&](auto& t) {
        const auto rows = h.rows[table++];
        // A handle is 32 bits and its largest value stands for none.
        if (ok && rows >= element_id::none_value)
            ok = std::unexpected(object_error::column_out_of_bounds);
        for_each_column([&](auto& c) {
            using value = std::remove_const_t<
                std::remove_reference_t<decltype(*c.values.data())>>;
            const auto offset = h.offsets[column++];
            if (!ok)
                return;
            ok = check(offset, rows, sizeof(value), h.size);
            if (ok)
                c.values = {reinterpret_cast<const value*>(
                    object.data() + offset), static_cast<std::size_t>(rows)};
        }, t);
    }, v);
    if (ok)
        ok = check(h.name_bytes_offset, h.name_bytes_size, 1, h.size);
    if (!ok)
        return std::unexpected(ok.error());
    v.name_bytes = {reinterpret_cast<const char*>(
        object.data() + h.name_bytes_offset),
        static_cast<std::size_t>(h.name_bytes_size)};
    return v;
}

bool checksum_matches(std::span<const std::byte> object) noexcept {
    object_header h;
    std::memcpy(&h, object.data(), sizeof(h));
    return h.checksum == checksum_of(object.subspan(checksum_first));
}

}

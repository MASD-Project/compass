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
#include "compass.store/ops/pack.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <numeric>
#include <string_view>
#include <vector>

namespace compass::store::ops {

namespace {

constexpr std::string_view header_magic = "CMPSPAK\n";
constexpr std::string_view footer_magic = "CMPSEND\n";
constexpr std::size_t fanout_size = 256 * 4;
constexpr std::size_t row_size = 48;
constexpr std::size_t footer_size = 32;
constexpr std::size_t key_size = 20;
constexpr std::uint8_t last_kind =
    static_cast<std::uint8_t>(types::entry_kind::result);

void put(std::string& out, std::uint64_t v, int bytes) {
    for (int i = 0; i < bytes; ++i)
        out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

std::uint64_t get(const std::byte* p, int bytes) noexcept {
    std::uint64_t v = 0;
    for (int i = bytes - 1; i >= 0; --i)
        v = (v << 8) | std::to_integer<std::uint8_t>(p[i]);
    return v;
}

const std::byte* row_of(const pack_view& p, std::uint64_t row) noexcept {
    return p.file.data() + p.index_offset + fanout_size + row * row_size;
}

std::uint64_t fanout(const pack_view& p, unsigned bucket) noexcept {
    return get(p.file.data() + p.index_offset + 4 * bucket, 4);
}

/**
 * @brief Compares the key and kind of a row with @p key and @p kind.
 */
int compare_row(const std::byte* row, const types::blob_id& key,
    std::uint8_t kind) noexcept {
    if (const auto c = std::memcmp(row, key.bytes.data(), key_size); c != 0)
        return c;
    const auto k = std::to_integer<std::uint8_t>(row[key_size]);
    return k < kind ? -1 : k > kind ? 1 : 0;
}

}

std::string encode_pack_header() {
    std::string out(header_magic);
    put(out, pack_version, 4);
    put(out, 0, 4);
    return out;
}

std::string encode_pack_index(std::span<const types::pack_entry> entries,
    std::uint64_t index_offset) {
    std::vector<std::size_t> order(entries.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    const auto same = [&](std::size_t a, std::size_t b) {
        return entries[a].key == entries[b].key &&
            entries[a].kind == entries[b].kind;
    };
    std::ranges::sort(order, [&](std::size_t a, std::size_t b) {
        const auto& x = entries[a];
        const auto& y = entries[b];
        if (x.key != y.key)
            return x.key < y.key;
        if (x.kind != y.kind)
            return x.kind < y.kind;
        if (x.written != y.written)
            return x.written > y.written;
        return a > b;
    });
    order.erase(std::ranges::unique(order, same).begin(), order.end());

    std::array<std::uint32_t, 256> counts{};
    for (const auto i : order)
        ++counts[entries[i].key.bytes[0]];
    std::string out;
    out.reserve(fanout_size + order.size() * row_size + footer_size);
    std::uint32_t total = 0;
    for (const auto c : counts) {
        total += c;
        put(out, total, 4);
    }
    for (const auto i : order) {
        const auto& e = entries[i];
        out.append(reinterpret_cast<const char*>(e.key.bytes.data()),
            key_size);
        put(out, static_cast<std::uint8_t>(e.kind), 1);
        put(out, 0, 3);
        put(out, e.offset, 8);
        put(out, e.size, 8);
        put(out, static_cast<std::uint64_t>(e.written), 8);
    }
    put(out, index_offset, 8);
    put(out, order.size(), 8);
    put(out, 0, 8);
    out += footer_magic;
    return out;
}

std::expected<pack_view, std::string> parse_pack(
    std::span<const std::byte> file) {
    using failure = std::unexpected<std::string>;
    const auto text = [&](std::size_t at, std::size_t n) {
        return std::string_view(
            reinterpret_cast<const char*>(file.data()) + at, n);
    };
    if (file.size() < pack_header_size + fanout_size + footer_size)
        return failure("too short to be a pack");
    if (text(0, header_magic.size()) != header_magic)
        return failure("not a pack");
    if (get(file.data() + 8, 4) != pack_version)
        return failure("pack version " +
            std::to_string(get(file.data() + 8, 4)));
    const auto footer = file.size() - footer_size;
    if (text(footer + 24, footer_magic.size()) != footer_magic)
        return failure("no footer");

    pack_view p{file, get(file.data() + footer, 8),
        get(file.data() + footer + 8, 8)};
    if (p.index_offset < pack_header_size ||
        p.index_offset % pack_alignment != 0 || p.index_offset > footer ||
        p.count > (footer - p.index_offset) / row_size ||
        p.index_offset + fanout_size + p.count * row_size != footer)
        return failure("the footer does not match the size");

    std::uint64_t row = 0;
    for (unsigned bucket = 0; bucket < 256; ++bucket) {
        const auto end = fanout(p, bucket);
        if (end < row || end > p.count)
            return failure("the fan-out table does not count the rows");
        for (; row < end; ++row) {
            const auto* r = row_of(p, row);
            const auto e = entry_at(p, row);
            if (std::to_integer<unsigned>(r[0]) != bucket)
                return failure("a row is in the wrong fan-out bucket");
            if (std::to_integer<std::uint8_t>(r[key_size]) > last_kind)
                return failure("a row has an unknown kind");
            if (row != 0 && compare_row(row_of(p, row - 1), e.key,
                    static_cast<std::uint8_t>(e.kind)) >= 0)
                return failure("the rows are out of order");
            if (e.offset < pack_header_size ||
                e.offset % pack_alignment != 0 || e.offset > p.index_offset ||
                e.size > p.index_offset - e.offset)
                return failure("an entry lies outside the pack");
        }
    }
    if (row != p.count)
        return failure("the fan-out table does not count the rows");
    return p;
}

types::pack_entry entry_at(const pack_view& p, std::uint64_t row) noexcept {
    const auto* r = row_of(p, row);
    types::pack_entry e;
    std::memcpy(e.key.bytes.data(), r, key_size);
    e.kind = static_cast<types::entry_kind>(
        std::to_integer<std::uint8_t>(r[key_size]));
    e.offset = get(r + 24, 8);
    e.size = get(r + 32, 8);
    e.written = static_cast<std::int64_t>(get(r + 40, 8));
    return e;
}

std::optional<types::pack_entry> find_entry(const pack_view& p,
    types::entry_kind kind, const types::blob_id& key) noexcept {
    const auto bucket = key.bytes[0];
    auto low = bucket == 0 ? 0 : fanout(p, bucket - 1u);
    auto high = fanout(p, bucket);
    const auto k = static_cast<std::uint8_t>(kind);
    while (low < high) {
        const auto mid = low + (high - low) / 2;
        const auto c = compare_row(row_of(p, mid), key, k);
        if (c == 0)
            return entry_at(p, mid);
        if (c < 0)
            low = mid + 1;
        else
            high = mid;
    }
    return std::nullopt;
}

std::size_t packs_to_merge(std::span<const std::uint64_t> ascending) noexcept {
    std::size_t merge = 0;
    std::uint64_t smaller = 0;
    for (std::size_t i = 0; i < ascending.size(); ++i) {
        if (ascending[i] / 2 < smaller)
            merge = i + 1;
        smaller += ascending[i];
    }
    return merge;
}

}

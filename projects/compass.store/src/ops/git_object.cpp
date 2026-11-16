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
#include "compass.store/ops/git_object.hpp"

#include <algorithm>
#include <charconv>
#include <climits>
#include <cstring>
#include <zlib.h>

namespace compass::store::ops {

namespace {

using failure = std::unexpected<std::string>;

int hex_digit(char c) noexcept {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

std::uint32_t big_endian_32(std::span<const std::byte> b,
    std::size_t at) noexcept {
    return std::to_integer<std::uint32_t>(b[at]) << 24 |
        std::to_integer<std::uint32_t>(b[at + 1]) << 16 |
        std::to_integer<std::uint32_t>(b[at + 2]) << 8 |
        std::to_integer<std::uint32_t>(b[at + 3]);
}

std::uint64_t big_endian_64(std::span<const std::byte> b,
    std::size_t at) noexcept {
    return std::uint64_t{big_endian_32(b, at)} << 32 | big_endian_32(b, at + 4);
}

/**
 * @brief A zlib inflate stream that ends itself.
 */
class inflater final {
public:
    inflater() { ok_ = inflateInit(&s_) == Z_OK; }
    ~inflater() {
        if (ok_)
            inflateEnd(&s_);
    }
    inflater(const inflater&) = delete;
    inflater& operator=(const inflater&) = delete;

    [[nodiscard]] bool ok() const noexcept { return ok_; }
    z_stream& stream() noexcept { return s_; }

    void input(std::span<const std::byte> in) noexcept {
        s_.next_in = reinterpret_cast<Bytef*>(
            const_cast<std::byte*>(in.data()));
        s_.avail_in = static_cast<uInt>(
            std::min<std::size_t>(in.size(), UINT_MAX));
    }

private:
    z_stream s_{};
    bool ok_ = false;
};

/**
 * @brief A little-endian base-128 number of a delta header at @p at of
 * @p delta, advancing @p at.
 */
std::optional<std::uint64_t> delta_size(std::string_view delta,
    std::size_t& at) noexcept {
    std::uint64_t value = 0;
    for (unsigned shift = 0; at < delta.size() && shift < 64; shift += 7) {
        const auto c = static_cast<std::uint8_t>(delta[at++]);
        value |= std::uint64_t{c & 0x7fu} << shift;
        if ((c & 0x80) == 0)
            return value;
    }
    return std::nullopt;
}

std::optional<types::object_type> type_named(std::string_view name) noexcept {
    if (name == "blob")
        return types::object_type::blob;
    if (name == "tree")
        return types::object_type::tree;
    if (name == "commit")
        return types::object_type::commit;
    if (name == "tag")
        return types::object_type::tag;
    return std::nullopt;
}

constexpr std::size_t index_header = 8;
constexpr std::size_t fanout_size = 256 * 4;
constexpr std::size_t id_size = 20;
constexpr std::size_t index_trailer = 2 * id_size;

}

std::optional<types::object_id> from_hex(std::string_view hex) noexcept {
    if (hex.size() != 2 * id_size)
        return std::nullopt;
    types::object_id id;
    for (std::size_t i = 0; i < id_size; ++i) {
        const auto high = hex_digit(hex[2 * i]);
        const auto low = hex_digit(hex[2 * i + 1]);
        if (high < 0 || low < 0)
            return std::nullopt;
        id.bytes[i] = static_cast<std::uint8_t>(high << 4 | low);
    }
    return id;
}

std::expected<std::string, std::string> inflate(
    std::span<const std::byte> in, std::size_t size) {
    inflater z;
    if (!z.ok())
        return failure("zlib could not start");
    z.input(in);
    // One spare byte, so a stream longer than size is caught rather than
    // cut short.
    std::string out(size + 1, '\0');
    auto& s = z.stream();
    s.next_out = reinterpret_cast<Bytef*>(out.data());
    s.avail_out = static_cast<uInt>(out.size());
    const auto rc = ::inflate(&s, Z_FINISH);
    if (rc != Z_STREAM_END)
        return failure(s.total_out > size
            ? "the stream inflates to more than " + std::to_string(size) +
                " bytes"
            : "the stream is damaged or cut short");
    if (s.total_out != size)
        return failure("the stream inflates to " +
            std::to_string(s.total_out) + " bytes, not " +
            std::to_string(size));
    out.resize(size);
    return out;
}

std::expected<types::git_object, std::string> parse_loose_object(
    std::span<const std::byte> file) {
    inflater z;
    if (!z.ok())
        return failure("zlib could not start");
    z.input(file);
    auto& s = z.stream();
    std::string out(std::max<std::size_t>(file.size() * 4, 64), '\0');
    for (;;) {
        s.next_out = reinterpret_cast<Bytef*>(out.data() + s.total_out);
        s.avail_out = static_cast<uInt>(out.size() - s.total_out);
        const auto rc = ::inflate(&s, Z_NO_FLUSH);
        if (rc == Z_STREAM_END)
            break;
        if (rc != Z_OK && !(rc == Z_BUF_ERROR && s.avail_out == 0))
            return failure("the stream is damaged or cut short");
        if (s.avail_out == 0)
            out.resize(out.size() * 2);
        else if (s.avail_in == 0)
            return failure("the stream is cut short");
    }
    out.resize(s.total_out);

    const auto nul = out.find('\0');
    const auto space = out.find(' ');
    if (nul == std::string::npos || space == std::string::npos || space > nul)
        return failure("no object header");
    const auto type = type_named(std::string_view(out).substr(0, space));
    if (!type)
        return failure("unknown object type " + out.substr(0, space));
    std::uint64_t size = 0;
    const auto* first = out.data() + space + 1;
    const auto* last = out.data() + nul;
    const auto [end, ec] = std::from_chars(first, last, size);
    if (ec != std::errc{} || end != last || first == last)
        return failure("bad object size");
    if (size != out.size() - nul - 1)
        return failure("the header names " + std::to_string(size) +
            " bytes and the object holds " +
            std::to_string(out.size() - nul - 1));
    out.erase(0, nul + 1);
    return types::git_object{*type, std::move(out)};
}

std::expected<std::string, std::string> apply_delta(std::string_view base,
    std::string_view delta) {
    std::size_t at = 0;
    const auto base_size = delta_size(delta, at);
    const auto result_size = delta_size(delta, at);
    if (!base_size || !result_size)
        return failure("the delta header is cut short");
    if (*base_size != base.size())
        return failure("the delta names a base of " +
            std::to_string(*base_size) + " bytes and the base has " +
            std::to_string(base.size()));
    std::string out;
    out.reserve(*result_size);
    while (at < delta.size()) {
        const auto op = static_cast<std::uint8_t>(delta[at++]);
        if (op & 0x80) {
            std::uint64_t offset = 0;
            std::uint64_t size = 0;
            for (unsigned i = 0; i < 4; ++i) {
                if ((op & (1u << i)) == 0)
                    continue;
                if (at == delta.size())
                    return failure("a copy is cut short");
                offset |= std::uint64_t{static_cast<std::uint8_t>(
                    delta[at++])} << (8 * i);
            }
            for (unsigned i = 0; i < 3; ++i) {
                if ((op & (0x10u << i)) == 0)
                    continue;
                if (at == delta.size())
                    return failure("a copy is cut short");
                size |= std::uint64_t{static_cast<std::uint8_t>(
                    delta[at++])} << (8 * i);
            }
            if (size == 0)
                size = 0x10000;
            if (offset > base.size() || size > base.size() - offset)
                return failure("a copy reaches past the end of the base");
            out.append(base.substr(offset, size));
        } else if (op != 0) {
            if (op > delta.size() - at)
                return failure("an insert is cut short");
            out.append(delta.substr(at, op));
            at += op;
        } else {
            return failure("the delta holds the reserved opcode 0");
        }
        if (out.size() > *result_size)
            return failure("the delta builds more than its result size");
    }
    if (out.size() != *result_size)
        return failure("the delta builds " + std::to_string(out.size()) +
            " bytes, not " + std::to_string(*result_size));
    return out;
}

std::expected<std::vector<types::tree_entry>, std::string> parse_tree(
    std::string_view tree) {
    std::vector<types::tree_entry> out;
    while (!tree.empty()) {
        const auto space = tree.find(' ');
        const auto nul = tree.find('\0');
        if (space == std::string_view::npos || nul == std::string_view::npos ||
            space > nul || space == 0 || nul == space + 1)
            return failure("a tree entry has no mode or name");
        if (tree.size() - nul - 1 < id_size)
            return failure("a tree entry is cut short");
        types::tree_entry e;
        const auto* first = tree.data();
        const auto [end, ec] = std::from_chars(first, first + space, e.mode, 8);
        if (ec != std::errc{} || end != first + space)
            return failure("a tree entry has a bad mode");
        e.name = tree.substr(space + 1, nul - space - 1);
        std::memcpy(e.id.bytes.data(), tree.data() + nul + 1, id_size);
        out.push_back(e);
        tree.remove_prefix(nul + 1 + id_size);
    }
    return out;
}

std::expected<types::object_id, std::string> tree_of_commit(
    std::string_view commit) {
    constexpr std::string_view prefix = "tree ";
    if (!commit.starts_with(prefix) ||
        commit.size() < prefix.size() + 2 * id_size + 1 ||
        commit[prefix.size() + 2 * id_size] != '\n')
        return failure("the commit does not start with its tree");
    const auto id = from_hex(commit.substr(prefix.size(), 2 * id_size));
    if (!id)
        return failure("the commit names a bad tree ID");
    return *id;
}

std::expected<pack_index, std::string> parse_pack_index(
    std::span<const std::byte> bytes) {
    if (bytes.size() < index_header + fanout_size + index_trailer)
        return failure("the pack index is cut short");
    if (big_endian_32(bytes, 0) != 0xff744f63u)
        return failure("the pack index is version 1, which is not read");
    if (const auto v = big_endian_32(bytes, 4); v != 2)
        return failure("the pack index is version " + std::to_string(v));
    pack_index r;
    r.fanout = bytes.subspan(index_header, fanout_size);
    std::uint32_t previous = 0;
    for (std::size_t i = 0; i < 256; ++i) {
        const auto n = big_endian_32(r.fanout, 4 * i);
        if (n < previous)
            return failure("the fan-out table of the pack index descends");
        previous = n;
    }
    r.count = previous;
    const std::size_t count = r.count;
    const auto tables = index_header + fanout_size;
    const auto needed = tables + count * (id_size + 4 + 4) + index_trailer;
    if (bytes.size() < needed)
        return failure("the pack index is cut short");
    r.ids = bytes.subspan(tables, count * id_size);
    r.offsets = bytes.subspan(tables + count * (id_size + 4), count * 4);
    const auto large = tables + count * (id_size + 8);
    r.large_offsets = bytes.subspan(large,
        (bytes.size() - large - index_trailer) / 8 * 8);
    return r;
}

std::optional<std::uint64_t> find_offset(const pack_index& index,
    const types::object_id& id) noexcept {
    const auto first = id.bytes[0];
    std::size_t lo = first == 0 ? 0 : big_endian_32(index.fanout, 4 * (first - 1));
    std::size_t hi = big_endian_32(index.fanout, 4 * first);
    while (lo < hi) {
        const auto mid = lo + (hi - lo) / 2;
        const auto c = std::memcmp(index.ids.data() + mid * id_size,
            id.bytes.data(), id_size);
        if (c < 0) {
            lo = mid + 1;
        } else if (c > 0) {
            hi = mid;
        } else {
            const auto word = big_endian_32(index.offsets, 4 * mid);
            if ((word & 0x80000000u) == 0)
                return word;
            const std::size_t row = word & 0x7fffffffu;
            if ((row + 1) * 8 > index.large_offsets.size())
                return std::nullopt;
            return big_endian_64(index.large_offsets, row * 8);
        }
    }
    return std::nullopt;
}

std::expected<pack_entry, std::string> parse_pack_entry(
    std::span<const std::byte> pack, std::uint64_t offset) {
    if (offset >= pack.size())
        return failure("the entry lies past the end of the pack");
    auto at = offset;
    auto c = std::to_integer<std::uint8_t>(pack[at++]);
    pack_entry e;
    const auto kind = (c >> 4) & 7u;
    if (kind == 0 || kind == 5)
        return failure("the entry has the unused kind " +
            std::to_string(kind));
    e.kind = static_cast<pack_entry_kind>(kind);
    e.size = c & 15u;
    for (unsigned shift = 4; c & 0x80; shift += 7) {
        if (at == pack.size() || shift > 57)
            return failure("the entry size is cut short");
        c = std::to_integer<std::uint8_t>(pack[at++]);
        e.size |= std::uint64_t{c & 0x7fu} << shift;
    }
    if (e.kind == pack_entry_kind::offset_delta) {
        if (at == pack.size())
            return failure("the delta offset is cut short");
        c = std::to_integer<std::uint8_t>(pack[at++]);
        std::uint64_t back = c & 0x7fu;
        while (c & 0x80) {
            if (at == pack.size() || back > (UINT64_MAX >> 8))
                return failure("the delta offset is cut short");
            c = std::to_integer<std::uint8_t>(pack[at++]);
            back = ((back + 1) << 7) | (c & 0x7fu);
        }
        if (back == 0 || back > offset)
            return failure("the delta base lies outside the pack");
        e.base_offset = offset - back;
    } else if (e.kind == pack_entry_kind::ref_delta) {
        if (pack.size() - at < id_size)
            return failure("the delta base ID is cut short");
        std::memcpy(e.base_id.bytes.data(), pack.data() + at, id_size);
        at += id_size;
    }
    e.data = at;
    return e;
}

}

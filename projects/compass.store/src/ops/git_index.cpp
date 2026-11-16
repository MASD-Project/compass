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
#include "compass.store/ops/git_index.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include "compass.store/ops/blob_hash.hpp"

namespace compass::store::ops {

namespace {

constexpr std::size_t hash_size = 20;
constexpr std::uint16_t extended_flag = 0x4000;
constexpr std::uint16_t intent_to_add_flag = 0x2000;
constexpr std::uint16_t known_extended_flags = 0x6000;
constexpr std::uint16_t name_mask = 0xFFF;

[[noreturn]] void reject(const std::string& why) {
    throw std::runtime_error("git index: " + why);
}

class reader final {
public:
    explicit reader(std::string_view bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] std::size_t position() const noexcept { return at_; }
    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - at_;
    }

    std::string_view take(std::size_t n) {
        if (remaining() < n)
            reject("truncated");
        const auto r = bytes_.substr(at_, n);
        at_ += n;
        return r;
    }

    std::uint32_t u32() {
        const auto b = take(4);
        return std::uint32_t{byte(b, 0)} << 24 | std::uint32_t{byte(b, 1)} << 16 |
            std::uint32_t{byte(b, 2)} << 8 | std::uint32_t{byte(b, 3)};
    }

    std::uint16_t u16() {
        const auto b = take(2);
        return static_cast<std::uint16_t>(byte(b, 0) << 8 | byte(b, 1));
    }

    /**
     * @brief The offset encoding of git's varint.c: seven bits a byte, most
     * significant first, with one added before each shift so that every
     * value has exactly one encoding.
     */
    std::uint64_t varint() {
        std::uint8_t c = byte(take(1), 0);
        std::uint64_t value = c & 0x7F;
        while ((c & 0x80) != 0) {
            if (value >= (std::uint64_t{1} << 56))
                reject("path prefix length overflows");
            c = byte(take(1), 0);
            value = ((value + 1) << 7) | (c & 0x7F);
        }
        return value;
    }

    std::string_view until_nul() {
        const auto nul = bytes_.find('\0', at_);
        if (nul == std::string_view::npos)
            reject("truncated path");
        const auto r = bytes_.substr(at_, nul - at_);
        at_ = nul + 1;
        return r;
    }

private:
    static std::uint8_t byte(std::string_view b, std::size_t i) noexcept {
        return static_cast<std::uint8_t>(b[i]);
    }

    std::string_view bytes_;
    std::size_t at_ = 0;
};

types::index_time read_time(reader& r) {
    types::index_time t;
    t.sec = r.u32();
    t.nsec = r.u32();
    return t;
}

void read_entry(reader& r, types::git_index& index, std::string& previous) {
    const auto start = r.position();
    types::stat_data s;
    s.ctime = read_time(r);
    s.mtime = read_time(r);
    s.dev = r.u32();
    s.ino = r.u32();
    const auto mode = r.u32();
    s.uid = r.u32();
    s.gid = r.u32();
    s.size = r.u32();
    types::blob_id id;
    std::memcpy(id.bytes.data(), r.take(hash_size).data(), hash_size);
    const auto flags = r.u16();

    std::uint8_t intent_to_add = 0;
    if ((flags & extended_flag) != 0) {
        if (index.version < 3)
            reject("extended flags in a version 2 index");
        const auto extended = r.u16();
        if ((extended & ~known_extended_flags) != 0)
            reject("unknown extended flags");
        intent_to_add = (extended & intent_to_add_flag) != 0 ? 1 : 0;
    }

    std::string path;
    if (index.version == 4) {
        const auto strip = r.varint();
        if (strip > previous.size())
            reject("path prefix longer than the previous path");
        path = previous.substr(0, previous.size() - strip);
        path += r.until_nul();
    } else {
        path = r.until_nul();
        const auto fixed = r.position() - 1 - start - path.size();
        const auto padded = (fixed + path.size() + 8) & ~std::size_t{7};
        const auto padding = r.take(padded - fixed - path.size() - 1);
        if (std::ranges::any_of(padding, [](char c) { return c != '\0'; }))
            reject("non-zero padding after " + path);
    }

    const std::size_t name_length = flags & name_mask;
    if (path.empty() || (name_length < name_mask && name_length != path.size()))
        reject("bad path length at entry " + std::to_string(index.size()));

    index.modes.push_back(mode);
    index.stats.push_back(s);
    index.ids.push_back(id);
    index.stages.push_back(static_cast<std::uint8_t>((flags >> 12) & 3));
    index.intent_to_add.push_back(intent_to_add);
    previous = path;
    index.paths.push_back(std::move(path));
}

bool optional_extension(std::string_view signature) noexcept {
    return (signature[0] >= 'A' && signature[0] <= 'Z') || signature == "sdir";
}

}

types::git_index parse_git_index(std::string_view bytes) {
    if (bytes.size() < 12 + hash_size)
        reject("truncated");
    const auto body = bytes.substr(0, bytes.size() - hash_size);
    const auto trailer = bytes.substr(body.size());
    if (std::ranges::any_of(trailer, [](char c) { return c != '\0'; })) {
        const auto digest = sha1_of(body);
        if (std::memcmp(digest.data(), trailer.data(), hash_size) != 0)
            reject("checksum mismatch");
    }

    reader r(body);
    if (r.take(4) != "DIRC")
        reject("no DIRC signature");
    types::git_index index;
    index.version = r.u32();
    if (index.version < 2 || index.version > 4)
        reject("unsupported version " + std::to_string(index.version));

    const auto count = r.u32();
    const auto plausible = std::min<std::size_t>(count, body.size() / 62);
    index.paths.reserve(plausible);
    index.modes.reserve(plausible);
    index.stats.reserve(plausible);
    index.ids.reserve(plausible);
    index.stages.reserve(plausible);
    index.intent_to_add.reserve(plausible);
    std::string previous;
    for (std::uint32_t i = 0; i < count; ++i)
        read_entry(r, index, previous);

    while (r.remaining() > 0) {
        const auto signature = r.take(4);
        const auto size = r.u32();
        r.take(size);
        if (!optional_extension(signature))
            reject("unsupported extension " + std::string(signature));
    }
    return index;
}

bool is_regular_file(std::uint32_t mode) noexcept {
    return (mode & 0170000) == 0100000;
}

bool is_unchanged(const types::stat_data& entry,
    const types::stat_data& file, types::index_time written) noexcept {
    return entry == file && written.sec > entry.mtime.sec;
}

bool is_clean(const types::stat_data& entry, const types::stat_data& file,
    types::index_time written) noexcept {
    return entry.size != 0 && is_unchanged(entry, file, written);
}

}

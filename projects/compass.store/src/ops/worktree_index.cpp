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
#include "compass.store/ops/worktree_index.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include "compass.store/ops/blob_hash.hpp"

namespace compass::store::ops {

namespace {

constexpr std::string_view magic = "CMPSWTI\n";
constexpr std::size_t hash_size = 20;

[[noreturn]] void reject(const std::string& why) {
    throw std::runtime_error("worktree index: " + why);
}

void put(std::string& out, std::uint32_t v) {
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<char>((v >> shift) & 0xFF));
}

void put(std::string& out, const types::index_time& t) {
    put(out, t.sec);
    put(out, t.nsec);
}

class reader final {
public:
    explicit reader(std::string_view bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] bool done() const noexcept { return at_ == bytes_.size(); }

    std::string_view take(std::size_t n) {
        if (bytes_.size() - at_ < n)
            reject("truncated");
        const auto r = bytes_.substr(at_, n);
        at_ += n;
        return r;
    }

    std::uint32_t u32() {
        const auto b = take(4);
        std::uint32_t v = 0;
        for (int i = 3; i >= 0; --i)
            v = (v << 8) | static_cast<std::uint8_t>(b[i]);
        return v;
    }

    types::index_time time() {
        types::index_time t;
        t.sec = u32();
        t.nsec = u32();
        return t;
    }

private:
    std::string_view bytes_;
    std::size_t at_ = 0;
};

}

std::string encode_worktree_index(const types::worktree_index& index) {
    std::string out(magic);
    put(out, worktree_index_version);
    put(out, static_cast<std::uint32_t>(index.size()));
    put(out, index.written);
    for (std::size_t i = 0; i < index.size(); ++i) {
        const auto& s = index.stats[i];
        put(out, s.ctime);
        put(out, s.mtime);
        for (const auto v : {s.dev, s.ino, s.uid, s.gid, s.size})
            put(out, v);
        out.append(reinterpret_cast<const char*>(index.ids[i].bytes.data()),
            hash_size);
        put(out, static_cast<std::uint32_t>(index.paths[i].size()));
        out += index.paths[i];
    }
    const auto digest = sha1_of(out);
    out.append(reinterpret_cast<const char*>(digest.data()), digest.size());
    return out;
}

types::worktree_index decode_worktree_index(std::string_view bytes) {
    if (bytes.size() < magic.size() + hash_size)
        reject("truncated");
    const auto body = bytes.substr(0, bytes.size() - hash_size);
    const auto digest = sha1_of(body);
    if (!std::ranges::equal(digest, bytes.substr(body.size()), {}, {},
            [](char c) { return static_cast<std::uint8_t>(c); }))
        reject("checksum does not match");

    reader r(body);
    if (r.take(magic.size()) != magic)
        reject("bad magic");
    if (const auto version = r.u32(); version != worktree_index_version)
        reject("version " + std::to_string(version) + " is not " +
            std::to_string(worktree_index_version));
    const auto count = r.u32();
    types::worktree_index index;
    index.written = r.time();
    for (std::uint32_t i = 0; i < count; ++i) {
        types::stat_data s;
        s.ctime = r.time();
        s.mtime = r.time();
        s.dev = r.u32();
        s.ino = r.u32();
        s.uid = r.u32();
        s.gid = r.u32();
        s.size = r.u32();
        types::blob_id id;
        std::ranges::copy(r.take(hash_size), id.bytes.begin());
        std::string path(r.take(r.u32()));
        if (path.empty() || (!index.paths.empty() && index.paths.back() >= path))
            reject("paths are not sorted and distinct");
        index.paths.push_back(std::move(path));
        index.stats.push_back(s);
        index.ids.push_back(id);
    }
    if (!r.done())
        reject("bytes after the last entry");
    return index;
}

}

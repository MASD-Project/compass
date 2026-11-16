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
#include "compass.codec/ops/outputs.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include "compass.store/ops/blob_hash.hpp"

namespace compass::codec::ops {

namespace {

namespace st = compass::store::types;

constexpr std::string_view magic = "CMPSOUT\n";
constexpr std::size_t hash_size = 20;

[[noreturn]] void reject(const std::string& why) {
    throw std::runtime_error("written outputs: " + why);
}

void put(std::string& out, std::uint32_t v) {
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<char>((v >> shift) & 0xFF));
}

void put(std::string& out, const st::index_time& t) {
    put(out, t.sec);
    put(out, t.nsec);
}

void put_text(std::string& out, std::string_view text) {
    put(out, static_cast<std::uint32_t>(text.size()));
    out += text;
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

    st::index_time time() {
        st::index_time t;
        t.sec = u32();
        t.nsec = u32();
        return t;
    }

    std::string_view text() { return take(u32()); }

private:
    std::string_view bytes_;
    std::size_t at_ = 0;
};

}

types::output_set outputs_of(const types::registry& r,
    const types::run_result& result) {
    types::output_set out;
    for (std::size_t c = 0; c < r.size(); ++c) {
        if (r.enabled[c] != 0)
            out.complete.emplace_back(r.codecs[c].name);
    }
    const auto formatted = [&](std::size_t c) {
        return c < result.formatted_ids.size() &&
            result.formatted_ids[c].has_value();
    };
    for (std::size_t c = 0; c < r.size(); ++c) {
        if (!formatted(c))
            continue;
        const auto& files = result.files[c];
        const auto& ids = *result.formatted_ids[c];
        for (std::uint32_t f = 0; f < files.size(); ++f)
            out.add(r.codecs[c].name, files.paths[types::file_id{f}],
                files.contents[types::file_id{f}], ids[f]);
    }
    std::vector<std::string_view> skipped;
    for (std::size_t c = 0; c < r.size(); ++c) {
        if (formatted(c))
            skipped.push_back(r.codecs[c].name);
    }
    const auto& record = result.record;
    for (std::uint32_t u = 0; u < record.size(); ++u) {
        const types::record_unit_id unit{u};
        if (std::ranges::contains(skipped, record.codecs[unit]))
            continue;
        const auto end = u + 1 < record.size()
            ? record.file_firsts[unit.next()].value
            : static_cast<std::uint32_t>(record.files.size());
        for (auto f = record.file_firsts[unit].value; f < end; ++f) {
            const types::file_id file{f};
            out.add(record.codecs[unit], record.files.paths[file],
                record.files.contents[file], record.file_hashes[file]);
        }
    }
    return out;
}

bool is_plain_relative(std::string_view path) noexcept {
    if (path.empty() || path.front() == '/')
        return false;
    for (std::size_t first = 0; first <= path.size();) {
        auto end = path.find('/', first);
        if (end == std::string_view::npos)
            end = path.size();
        const auto part = path.substr(first, end - first);
        if (part.empty() || part == "." || part == "..")
            return false;
        first = end + 1;
    }
    return true;
}

std::vector<std::string> check_outputs(const types::output_set& outputs) {
    std::vector<std::string> problems;
    std::vector<std::uint32_t> order(outputs.size());
    std::iota(order.begin(), order.end(), 0u);
    const auto path = [&](std::uint32_t i) {
        return outputs.files.paths[types::file_id{i}];
    };
    const auto codec = [&](std::uint32_t i) {
        return outputs.codecs[types::file_id{i}];
    };
    std::ranges::stable_sort(order, {}, path);
    for (std::size_t k = 0; k < order.size(); ++k) {
        const auto i = order[k];
        if (!is_plain_relative(path(i)))
            problems.push_back(std::string(codec(i)) + " produced \"" +
                std::string(path(i)) + "\", which is not a plain path "
                "under the output root");
        if (k > 0 && path(order[k - 1]) == path(i))
            problems.push_back(std::string(codec(order[k - 1])) + " and " +
                std::string(codec(i)) + " both produced \"" +
                std::string(path(i)) + "\"");
    }
    return problems;
}

std::string encode_written_outputs(const types::written_outputs& record) {
    std::string out(magic);
    put(out, written_outputs_version);
    put(out, static_cast<std::uint32_t>(record.size()));
    put(out, record.written);
    put_text(out, record.root);
    for (std::size_t i = 0; i < record.size(); ++i) {
        put_text(out, record.paths[i]);
        put_text(out, record.codecs[i]);
        out.append(reinterpret_cast<const char*>(record.hashes[i].bytes.data()),
            hash_size);
        const auto& s = record.stats[i];
        put(out, s.ctime);
        put(out, s.mtime);
        for (const auto v : {s.dev, s.ino, s.uid, s.gid, s.size})
            put(out, v);
    }
    const auto digest = compass::store::ops::sha1_of(out);
    out.append(reinterpret_cast<const char*>(digest.data()), digest.size());
    return out;
}

types::written_outputs decode_written_outputs(std::string_view bytes) {
    if (bytes.size() < magic.size() + hash_size)
        reject("truncated");
    const auto body = bytes.substr(0, bytes.size() - hash_size);
    const auto digest = compass::store::ops::sha1_of(body);
    if (!std::ranges::equal(digest, bytes.substr(body.size()), {}, {},
            [](char c) { return static_cast<std::uint8_t>(c); }))
        reject("checksum does not match");

    reader r(body);
    if (r.take(magic.size()) != magic)
        reject("bad magic");
    if (const auto version = r.u32(); version != written_outputs_version)
        reject("version " + std::to_string(version) + " is not " +
            std::to_string(written_outputs_version));
    const auto count = r.u32();
    types::written_outputs record;
    record.written = r.time();
    record.root = r.text();
    for (std::uint32_t i = 0; i < count; ++i) {
        std::string path(r.text());
        if (!is_plain_relative(path))
            reject("\"" + path + "\" is not a plain relative path");
        if (!record.paths.empty() && record.paths.back() >= path)
            reject("paths are not sorted and distinct");
        record.paths.push_back(std::move(path));
        record.codecs.emplace_back(r.text());
        st::blob_id id;
        std::ranges::copy(r.take(hash_size), id.bytes.begin());
        record.hashes.push_back(id);
        st::stat_data s;
        s.ctime = r.time();
        s.mtime = r.time();
        s.dev = r.u32();
        s.ino = r.u32();
        s.uid = r.u32();
        s.gid = r.u32();
        s.size = r.u32();
        record.stats.push_back(s);
    }
    if (!r.done())
        reject("bytes after the last entry");
    return record;
}

}

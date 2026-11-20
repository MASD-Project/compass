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
#include "compass.codec/ops/action_cache.hpp"

#include <algorithm>
#include <numeric>
#include <tuple>
#include <vector>
#include "compass.store/ops/blob_hash.hpp"

namespace compass::codec::ops {

namespace {

using compass::store::types::blob_id;

constexpr std::size_t hash_size = 20;

/**
 * @brief What an encoded stage, an encoded scan table, a fact object and an
 * input chunk start with, "STG1", "SCN3", "FCT1" and "INP1" little-endian,
 * so none is read as another nor as the table an earlier build wrote,
 * which held every file.
 */
constexpr std::uint32_t stage_magic = 0x31475453;
constexpr std::uint32_t scan_magic = 0x334E4353;
constexpr std::uint32_t scan_fact_magic = 0x31544346;
constexpr std::uint32_t input_chunk_magic = 0x31504E49;

void put(std::string& out, std::uint8_t v) {
    out.push_back(static_cast<char>(v));
}

void put(std::string& out, std::uint32_t v) {
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<char>((v >> shift) & 0xFF));
}

void put(std::string& out, const blob_id& id) {
    out.append(reinterpret_cast<const char*>(id.bytes.data()), hash_size);
}

void put_text(std::string& out, std::string_view text) {
    put(out, static_cast<std::uint32_t>(text.size()));
    out += text;
}

/**
 * @brief Reads what put wrote; nothing once any read ran past the end.
 */
class reader final {
public:
    explicit reader(std::string_view bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] bool ok() const noexcept { return ok_; }
    [[nodiscard]] bool done() const noexcept {
        return ok_ && at_ == bytes_.size();
    }

    std::string_view take(std::size_t n) {
        if (!ok_ || bytes_.size() - at_ < n) {
            ok_ = false;
            return {};
        }
        const auto r = bytes_.substr(at_, n);
        at_ += n;
        return r;
    }

    std::uint8_t u8() {
        const auto b = take(1);
        return b.empty() ? 0 : static_cast<std::uint8_t>(b[0]);
    }

    std::uint32_t u32() {
        const auto b = take(4);
        std::uint32_t v = 0;
        for (int i = static_cast<int>(b.size()) - 1; i >= 0; --i)
            v = (v << 8) | static_cast<std::uint8_t>(b[i]);
        return v;
    }

    std::string_view text() { return take(u32()); }

    blob_id id() {
        blob_id r;
        const auto b = take(hash_size);
        if (b.size() == hash_size)
            std::ranges::copy(b, r.bytes.begin());
        return r;
    }

private:
    std::string_view bytes_;
    std::size_t at_ = 0;
    bool ok_ = true;
};

template <typename Column>
std::uint32_t end_of(const Column& firsts, std::uint32_t row,
    std::size_t total) noexcept {
    return row + 1 < firsts.size() ? firsts.values[row + 1].value
        : static_cast<std::uint32_t>(total);
}

blob_id sha1(std::string_view bytes) {
    return {compass::store::ops::sha1_of(bytes)};
}

/**
 * @brief The rows of the inputs of unit @p u of @p record, sorted by kind
 * and key.
 */
std::vector<std::uint32_t> sorted_inputs(
    const types::dependency_record& record, types::record_unit_id u) {
    const auto begin = record.input_firsts[u].value;
    const auto end = end_of(record.input_firsts, u.value,
        record.input_count());
    std::vector<std::uint32_t> order(end - begin);
    std::iota(order.begin(), order.end(), begin);
    std::ranges::sort(order, {}, [&](std::uint32_t i) {
        return std::tuple(record.input_kinds[types::input_id{i}],
            record.input_keys[types::input_id{i}]);
    });
    return order;
}

void put_input(std::string& out, const types::dependency_record& record,
    types::input_id in) {
    put(out, static_cast<std::uint8_t>(record.input_kinds[in]));
    put_text(out, record.input_keys[in]);
    put(out, record.input_found[in]);
    put(out, record.input_values[in]);
}

/**
 * @brief FNV-1a of the input's kind and key, finished with the mix of
 * splitmix64 so its low bits are as good as its high ones.
 */
std::uint64_t cut_hash(types::input_kind kind, std::string_view key) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    const auto add = [&](std::uint8_t b) {
        h ^= b;
        h *= 0x100000001b3ULL;
    };
    add(static_cast<std::uint8_t>(kind));
    for (const char c : key)
        add(static_cast<std::uint8_t>(c));
    h ^= h >> 30;
    h *= 0xbf58476d1ce4e5b9ULL;
    h ^= h >> 27;
    h *= 0x94d049bb133111ebULL;
    h ^= h >> 31;
    return h;
}

}

blob_id manifest_key(std::string_view codec, std::uint32_t version,
    const blob_id& configuration, std::string_view document,
    const blob_id& subject) {
    std::string bytes;
    put(bytes, action_format_version);
    put_text(bytes, codec);
    put(bytes, version);
    put(bytes, configuration);
    put_text(bytes, document);
    put(bytes, subject);
    return sha1(bytes);
}

blob_id result_key(const blob_id& manifest,
    const types::dependency_record& record, types::record_unit_id u) {
    std::string bytes;
    put(bytes, manifest);
    for (const auto i : sorted_inputs(record, u))
        put_input(bytes, record, types::input_id{i});
    return sha1(bytes);
}

std::vector<input_chunk> chunk_input_set(
    const types::dependency_record& record, types::record_unit_id u) {
    std::vector<input_chunk> chunks;
    const auto begin = record.input_firsts[u].value;
    const auto end = end_of(record.input_firsts, u.value,
        record.input_count());
    auto first = begin;
    for (auto k = begin; k < end; ++k) {
        const types::input_id in{k};
        const bool last = k + 1 == end || k + 1 - first == chunk_limit ||
            cut_hash(record.input_kinds[in], record.input_keys[in]) %
                    chunk_spacing == 0;
        if (!last)
            continue;
        std::string body;
        put(body, input_chunk_magic);
        put(body, k + 1 - first);
        for (auto j = first; j <= k; ++j)
            put_input(body, record, types::input_id{j});
        chunks.push_back({sha1(body), std::move(body)});
        first = k + 1;
    }
    return chunks;
}

bool decode_input_chunk(std::string_view body, types::input_list& to) {
    reader r(body);
    if (r.u32() != input_chunk_magic)
        return false;
    const auto inputs = r.u32();
    for (std::uint32_t i = 0; i < inputs && r.ok(); ++i) {
        const auto kind = r.u8();
        if (kind > static_cast<std::uint8_t>(types::last_input_kind))
            return false;
        to.input_kinds.values.push_back(static_cast<types::input_kind>(kind));
        to.input_keys.add(r.text());
        to.input_found.values.push_back(r.u8());
        to.input_values.values.push_back(r.id());
    }
    return r.done();
}

bool add_input_set(types::action_manifest& m, const blob_id& result,
    std::span<const input_chunk> chunks) {
    const auto same = [&](std::uint32_t s) {
        const auto first = m.chunk_firsts.values[s].value;
        const auto last = end_of(m.chunk_firsts, s, m.chunk_count());
        return m.results.values[s] == result &&
            std::ranges::equal(
                std::span(m.chunks.values).subspan(first, last - first),
                chunks, {}, {}, &input_chunk::key);
    };
    if (m.size() != 0 && same(0))
        return false;

    types::action_manifest next;
    const auto open = [&](const blob_id& key) {
        next.results.values.push_back(key);
        next.chunk_firsts.values.push_back(
            {static_cast<std::uint32_t>(next.chunk_count())});
    };
    open(result);
    for (const auto& c : chunks)
        next.chunks.values.push_back(c.key);
    for (std::uint32_t s = 0;
         s < m.size() && next.size() < manifest_capacity; ++s) {
        if (same(s))
            continue;
        open(m.results.values[s]);
        const auto last = end_of(m.chunk_firsts, s, m.chunk_count());
        for (auto c = m.chunk_firsts.values[s].value; c < last; ++c)
            next.chunks.values.push_back(m.chunks.values[c]);
    }
    m = std::move(next);
    return true;
}

std::string encode_manifest(const types::action_manifest& m) {
    std::string out;
    put(out, action_format_version);
    put(out, static_cast<std::uint32_t>(m.size()));
    for (std::uint32_t s = 0; s < m.size(); ++s) {
        put(out, m.results.values[s]);
        const auto first = m.chunk_firsts.values[s].value;
        const auto last = end_of(m.chunk_firsts, s, m.chunk_count());
        put(out, last - first);
        for (auto c = first; c < last; ++c)
            put(out, m.chunks.values[c]);
    }
    return out;
}

std::optional<types::action_manifest> decode_manifest(std::string_view body) {
    reader r(body);
    if (r.u32() != action_format_version)
        return std::nullopt;
    types::action_manifest m;
    const auto sets = r.u32();
    for (std::uint32_t s = 0; s < sets && r.ok(); ++s) {
        m.results.values.push_back(r.id());
        m.chunk_firsts.values.push_back(
            {static_cast<std::uint32_t>(m.chunk_count())});
        const auto chunks = r.u32();
        for (std::uint32_t c = 0; c < chunks && r.ok(); ++c)
            m.chunks.values.push_back(r.id());
    }
    if (!r.done())
        return std::nullopt;
    return m;
}

std::string encode_result(const types::dependency_record& record,
    types::record_unit_id u, std::string_view scan) {
    std::string out;
    put(out, action_format_version);
    const auto begin = record.file_firsts[u].value;
    const auto end = end_of(record.file_firsts, u.value,
        record.files.size());
    put(out, end - begin);
    for (auto f = begin; f < end; ++f) {
        const types::file_id file{f};
        put_text(out, record.files.paths[file]);
        put_text(out, record.files.contents[file]);
    }
    put_text(out, record.diagnostics[u]);
    put_text(out, scan);
    return out;
}

std::optional<action_result> decode_result(std::string_view body) {
    reader r(body);
    if (r.u32() != action_format_version)
        return std::nullopt;
    action_result result;
    const auto files = r.u32();
    for (std::uint32_t f = 0; f < files && r.ok(); ++f) {
        const auto path = r.text();
        result.files.add(path, r.text());
    }
    result.diagnostics = r.text();
    result.scan = r.text();
    if (!r.done())
        return std::nullopt;
    return result;
}

blob_id scan_fact_key(std::string_view codec, std::uint32_t version,
    const blob_id& configuration, std::string_view path,
    const blob_id& content) {
    std::string bytes;
    put(bytes, scan_fact_magic);
    put_text(bytes, codec);
    put(bytes, version);
    put(bytes, configuration);
    put_text(bytes, path);
    put(bytes, content);
    return sha1(bytes);
}

std::string encode_scan_fact(const scan_fact& f) {
    std::string out;
    out.reserve(12 + f.fact.size() + f.findings.size());
    put(out, scan_fact_magic);
    put_text(out, f.fact);
    put_text(out, f.findings);
    return out;
}

std::optional<scan_fact> decode_scan_fact(std::string_view bytes) {
    reader r(bytes);
    if (r.u32() != scan_fact_magic)
        return std::nullopt;
    scan_fact out;
    out.fact = r.text();
    out.findings = r.text();
    if (!r.done())
        return std::nullopt;
    return out;
}

blob_id scan_digest(std::span<const types::scanned_file> files) {
    std::string bytes;
    for (const auto& f : files) {
        put_text(bytes, f.path);
        put_text(bytes, f.fact);
    }
    return sha1(bytes);
}

std::string encode_scan(const scan_table& t) {
    std::string out;
    put(out, scan_magic);
    put(out, t.files);
    put(out, t.digest);
    put(out, static_cast<std::uint32_t>(t.read_by_reduce.size()));
    for (const auto path : t.read_by_reduce)
        put_text(out, path);
    put_text(out, t.reduce_findings);
    return out;
}

std::optional<scan_table> decode_scan(std::string_view bytes) {
    reader r(bytes);
    if (r.u32() != scan_magic)
        return std::nullopt;
    scan_table out;
    out.files = r.u32();
    out.digest = r.id();
    const auto read = r.u32();
    for (std::uint32_t i = 0; i < read && r.ok(); ++i)
        out.read_by_reduce.push_back(r.text());
    out.reduce_findings = r.text();
    if (!r.done())
        return std::nullopt;
    return out;
}

std::string encode_stage(const types::stage& s) {
    std::string out;
    put(out, stage_magic);
    put(out, static_cast<std::uint32_t>(s.parts.size()));
    for (const auto& part : s.parts)
        put_text(out, part);
    put(out, static_cast<std::uint32_t>(s.facts.size()));
    for (const auto& [name, value] : s.facts) {
        put_text(out, name);
        put(out, value);
    }
    return out;
}

std::optional<types::stage> decode_stage(std::string_view bytes) {
    reader r(bytes);
    if (r.u32() != stage_magic)
        return std::nullopt;
    types::stage s;
    const auto parts = r.u32();
    for (std::uint32_t i = 0; i < parts && r.ok(); ++i)
        s.parts.emplace_back(r.text());
    const auto facts = r.u32();
    for (std::uint32_t i = 0; i < facts && r.ok(); ++i) {
        std::string name(r.text());
        s.facts.emplace_back(std::move(name), r.id());
    }
    if (!r.done())
        return std::nullopt;
    return s;
}

}

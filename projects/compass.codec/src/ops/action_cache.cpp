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
 * @brief The input columns of a record or a manifest, from begin to end.
 */
template <typename Table>
void put_inputs(std::string& out, const Table& t, std::uint32_t begin,
    std::uint32_t end) {
    put(out, end - begin);
    for (auto i = begin; i < end; ++i) {
        const types::input_id in{i};
        put(out, static_cast<std::uint8_t>(t.input_kinds[in]));
        put_text(out, t.input_keys[in]);
        put(out, t.input_found[in]);
        put(out, t.input_values[in]);
    }
}

template <typename Table>
void add_input(types::action_manifest& to, const Table& from,
    types::input_id in) {
    to.input_kinds.values.push_back(from.input_kinds[in]);
    to.input_keys.add(from.input_keys[in]);
    to.input_found.values.push_back(from.input_found[in]);
    to.input_values.values.push_back(from.input_values[in]);
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
    const auto begin = record.input_firsts[u].value;
    const auto end = end_of(record.input_firsts, u.value,
        record.input_count());
    std::vector<std::uint32_t> order(end - begin);
    std::iota(order.begin(), order.end(), begin);
    std::ranges::sort(order, {}, [&](std::uint32_t i) {
        return std::tuple(record.input_kinds[types::input_id{i}],
            record.input_keys[types::input_id{i}]);
    });
    std::string bytes;
    put(bytes, manifest);
    for (const auto i : order) {
        const types::input_id in{i};
        put(bytes, static_cast<std::uint8_t>(record.input_kinds[in]));
        put_text(bytes, record.input_keys[in]);
        put(bytes, record.input_found[in]);
        put(bytes, record.input_values[in]);
    }
    return sha1(bytes);
}

bool add_input_set(types::action_manifest& m,
    const types::dependency_record& record, types::record_unit_id u) {
    const auto begin = record.input_firsts[u].value;
    const auto end = end_of(record.input_firsts, u.value,
        record.input_count());
    const auto result = record.results[u];
    const auto same = [&](std::uint32_t s) {
        const auto first = m.input_firsts.values[s].value;
        const auto last = end_of(m.input_firsts, s, m.input_count());
        if (last - first != end - begin || m.results.values[s] != result)
            return false;
        for (std::uint32_t k = 0; k < end - begin; ++k) {
            const types::input_id a{first + k};
            const types::input_id b{begin + k};
            if (m.input_kinds[a] != record.input_kinds[b] ||
                m.input_keys[a] != record.input_keys[b] ||
                m.input_found[a] != record.input_found[b] ||
                m.input_values[a] != record.input_values[b])
                return false;
        }
        return true;
    };
    if (m.size() != 0 && same(0))
        return false;

    types::action_manifest next;
    const auto open = [&](const blob_id& key) {
        next.input_firsts.values.push_back(
            {static_cast<std::uint32_t>(next.input_count())});
        next.results.values.push_back(key);
    };
    open(result);
    for (auto i = begin; i < end; ++i)
        add_input(next, record, types::input_id{i});
    for (std::uint32_t s = 0;
         s < m.size() && next.size() < manifest_capacity; ++s) {
        if (same(s))
            continue;
        open(m.results.values[s]);
        const auto last = end_of(m.input_firsts, s, m.input_count());
        for (auto i = m.input_firsts.values[s].value; i < last; ++i)
            add_input(next, m, types::input_id{i});
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
        put_inputs(out, m, m.input_firsts.values[s].value,
            end_of(m.input_firsts, s, m.input_count()));
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
        m.input_firsts.values.push_back(
            {static_cast<std::uint32_t>(m.input_count())});
        m.results.values.push_back(r.id());
        const auto inputs = r.u32();
        for (std::uint32_t i = 0; i < inputs && r.ok(); ++i) {
            const auto kind = r.u8();
            if (kind > static_cast<std::uint8_t>(
                    types::last_input_kind))
                return std::nullopt;
            m.input_kinds.values.push_back(
                static_cast<types::input_kind>(kind));
            m.input_keys.add(r.text());
            m.input_found.values.push_back(r.u8());
            m.input_values.values.push_back(r.id());
        }
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

void put_scan_entry(std::string& out, std::string_view path,
    const scan_entry& e) {
    put_text(out, path);
    put(out, e.content);
    put_text(out, e.fact);
    put_text(out, e.findings);
}

std::optional<std::unordered_map<std::string_view, scan_entry>>
decode_scan(std::string_view bytes) {
    reader r(bytes);
    std::unordered_map<std::string_view, scan_entry> out;
    while (r.ok() && !r.done()) {
        const auto path = r.text();
        scan_entry e;
        e.content = r.id();
        e.fact = r.text();
        e.findings = r.text();
        out.insert_or_assign(path, e);
    }
    if (!r.done())
        return std::nullopt;
    return out;
}

}

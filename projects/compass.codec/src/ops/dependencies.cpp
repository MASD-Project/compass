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
#include "compass.codec/ops/dependencies.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>
#include "compass.codec/ops/registry.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::codec::ops {

namespace {

namespace ot = compass::org::types;
using compass::store::types::blob_id;

constexpr std::string_view magic = "CMPSDEP\n";
constexpr std::size_t hash_size = 20;

[[noreturn]] void reject(const std::string& why) {
    throw std::runtime_error("dependency record: " + why);
}

void put(std::string& out, std::uint8_t v) {
    out.push_back(static_cast<char>(v));
}

void put(std::string& out, std::uint32_t v) {
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<char>((v >> shift) & 0xFF));
}

void put(std::string& out, std::uint64_t v) {
    for (int shift = 0; shift < 64; shift += 8)
        out.push_back(static_cast<char>((v >> shift) & 0xFF));
}

void put(std::string& out, const blob_id& id) {
    out.append(reinterpret_cast<const char*>(id.bytes.data()), hash_size);
}

void put_text(std::string& out, std::string_view text) {
    put(out, static_cast<std::uint64_t>(text.size()));
    out += text;
}

template <typename Value>
void put_values(std::string& out, const std::vector<Value>& values) {
    put(out, static_cast<std::uint64_t>(values.size()));
    out.append(reinterpret_cast<const char*>(values.data()),
        values.size() * sizeof(Value));
}

blob_id sha1(std::string_view bytes) {
    return {compass::store::ops::sha1_of(bytes)};
}

class reader final {
public:
    explicit reader(std::string_view bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] bool done() const noexcept { return at_ == bytes_.size(); }

    std::string_view take(std::uint64_t n) {
        if (bytes_.size() - at_ < n)
            reject("truncated");
        const auto r = bytes_.substr(at_, n);
        at_ += n;
        return r;
    }

    std::uint64_t number(int bytes) {
        const auto b = take(bytes);
        std::uint64_t v = 0;
        for (int i = bytes - 1; i >= 0; --i)
            v = (v << 8) | static_cast<std::uint8_t>(b[i]);
        return v;
    }

    std::uint8_t u8() { return static_cast<std::uint8_t>(number(1)); }
    std::uint32_t u32() { return static_cast<std::uint32_t>(number(4)); }
    std::uint64_t u64() { return number(8); }
    std::string_view text() { return take(u64()); }

    blob_id id() {
        blob_id r;
        std::ranges::copy(take(hash_size), r.bytes.begin());
        return r;
    }

private:
    std::string_view bytes_;
    std::size_t at_ = 0;
};

}

types::corpus_index index_corpus(const types::corpus& c) {
    types::corpus_index index;
    index.paths.reserve(c.size());
    for (std::uint32_t row = 0; row < c.size(); ++row) {
        index.paths.emplace(c.paths[row], ot::document_id{row});
        const auto& tables = c.tables[row];
        const auto p = org::ops::find_document_property(tables, "ID");
        if (p.is_none())
            continue;
        const auto& source = c.sources[row];
        const auto id = org::ops::trimmed(source,
            tables.properties.values[p]).of(source);
        if (!id.empty())
            index.ids.emplace(id, ot::document_id{row});
    }
    return index;
}

blob_id configuration_hash(const types::registry& r, types::codec_id codec) {
    std::string bytes;
    const auto values = option_values(r, codec);
    const auto& options = r.codecs[codec.value].options;
    for (std::size_t i = 0; i < options.size(); ++i) {
        put_text(bytes, options[i].key);
        put_text(bytes, values[i]);
    }
    return sha1(bytes);
}

blob_id registry_hash(const ot::type_registry& types) {
    std::string bytes;
    const auto& t = types.types;
    put_values(bytes, t.names.values);
    put_values(bytes, t.kinds.values);
    put_values(bytes, t.levels.values);
    put_values(bytes, t.folders.values);
    put_values(bytes, t.todo_firsts.values);
    put_values(bytes, t.done_firsts.values);
    put_values(bytes, t.section_firsts.values);
    put_values(bytes, types.entries.names.values);
    put_values(bytes, types.names.firsts.values);
    put_values(bytes, types.name_bytes);
    return sha1(bytes);
}

blob_id type_set_hash(const types::corpus& c,
    std::span<const ot::document_id> rows) {
    std::vector<ot::document_id> sorted(rows.begin(), rows.end());
    std::ranges::sort(sorted, {},
        [&](ot::document_id row) { return c.paths[row.value]; });
    std::string bytes;
    for (const auto row : sorted) {
        put_text(bytes, c.paths[row.value]);
        put(bytes, c.contents[row.value]);
    }
    return sha1(bytes);
}

std::string encode_diagnostics(const types::corpus& c,
    const types::diagnostic_table& t, std::uint32_t begin,
    std::uint32_t end) {
    std::string out;
    for (auto i = begin; i < end; ++i) {
        const types::diagnostic_id d{i};
        const auto document = t.documents[d];
        put(out, static_cast<std::uint8_t>(document.is_none() ? 1 : 0));
        put_text(out, document.is_none() ? t.files[d]
                                         : c.paths[document.value]);
        put(out, t.ranges[d].begin);
        put(out, t.ranges[d].end);
        put(out, t.rules[d].value);
        put(out, static_cast<std::uint8_t>(t.levels[d]));
        put_text(out, t.messages[d]);
        put_text(out, t.fixes[d]);
    }
    return out;
}

bool decode_diagnostics(std::string_view bytes,
    const types::corpus_index& index, types::diagnostic_table* out) {
    try {
        reader r(bytes);
        while (!r.done()) {
            const auto is_file = r.u8() != 0;
            const auto path = r.text();
            types::diagnostic d;
            if (is_file) {
                d.document = ot::document_id::none();
                d.file = path;
            } else {
                const auto it = index.paths.find(path);
                if (it == index.paths.end())
                    return false;
                d.document = it->second;
            }
            d.range.begin = r.u32();
            d.range.end = r.u32();
            d.rule = types::rule_id{r.u32()};
            d.level = static_cast<types::severity>(r.u8());
            d.message = r.text();
            d.fix = r.text();
            if (out != nullptr)
                out->add(d);
        }
    } catch (const std::runtime_error&) {
        return false;
    }
    return true;
}

std::string encode_dependency_record(const types::dependency_record& record) {
    std::string out(magic);
    put(out, dependency_record_version);
    put(out, static_cast<std::uint32_t>(record.size()));
    for (std::uint32_t u = 0; u < record.size(); ++u) {
        const types::record_unit_id unit{u};
        put_text(out, record.codecs[unit]);
        put(out, record.versions[unit]);
        put(out, record.configurations[unit]);
        put_text(out, record.documents[unit]);

        const auto input_end = u + 1 < record.size()
            ? record.input_firsts[unit.next()].value
            : static_cast<std::uint32_t>(record.input_count());
        const auto input_begin = record.input_firsts[unit].value;
        put(out, input_end - input_begin);
        for (auto i = input_begin; i < input_end; ++i) {
            const types::input_id in{i};
            put(out, static_cast<std::uint8_t>(record.input_kinds[in]));
            put_text(out, record.input_keys[in]);
            put(out, record.input_found[in]);
            put(out, record.input_values[in]);
        }

        const auto file_end = u + 1 < record.size()
            ? record.file_firsts[unit.next()].value
            : static_cast<std::uint32_t>(record.files.size());
        const auto file_begin = record.file_firsts[unit].value;
        put(out, file_end - file_begin);
        for (auto f = file_begin; f < file_end; ++f) {
            const types::file_id file{f};
            put_text(out, record.files.paths[file]);
            put(out, record.file_hashes[file]);
            put_text(out, record.files.contents[file]);
        }

        put(out, record.diagnostic_counts[unit]);
        put(out, record.diagnostic_hashes[unit]);
        put_text(out, record.diagnostics[unit]);
        put(out, record.manifests[unit]);
        put(out, record.results[unit]);
    }
    const auto digest = compass::store::ops::sha1_of(out);
    out.append(reinterpret_cast<const char*>(digest.data()), digest.size());
    return out;
}

types::dependency_record decode_dependency_record(std::string_view bytes) {
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
    if (const auto version = r.u32(); version != dependency_record_version)
        reject("version " + std::to_string(version) + " is not " +
            std::to_string(dependency_record_version));

    types::dependency_record record;
    const auto units = r.u32();
    for (std::uint32_t u = 0; u < units; ++u) {
        record.codecs.add(r.text());
        record.versions.values.push_back(r.u32());
        record.configurations.values.push_back(r.id());
        record.documents.add(r.text());

        record.input_firsts.values.push_back(
            {static_cast<std::uint32_t>(record.input_count())});
        const auto inputs = r.u32();
        for (std::uint32_t i = 0; i < inputs; ++i) {
            const auto kind = r.u8();
            if (kind > static_cast<std::uint8_t>(
                    types::last_input_kind))
                reject("unknown input kind " + std::to_string(kind));
            record.input_kinds.values.push_back(
                static_cast<types::input_kind>(kind));
            record.input_keys.add(r.text());
            record.input_found.values.push_back(r.u8());
            record.input_values.values.push_back(r.id());
        }

        record.file_firsts.values.push_back(
            {static_cast<std::uint32_t>(record.files.size())});
        const auto files = r.u32();
        for (std::uint32_t f = 0; f < files; ++f) {
            const auto path = r.text();
            record.file_hashes.values.push_back(r.id());
            record.files.add(path, r.text());
        }

        record.diagnostic_counts.values.push_back(r.u32());
        record.diagnostic_hashes.values.push_back(r.id());
        record.diagnostics.add(r.text());
        record.manifests.values.push_back(r.id());
        record.results.values.push_back(r.id());
    }
    if (!r.done())
        reject("bytes after the last unit");
    return record;
}

}

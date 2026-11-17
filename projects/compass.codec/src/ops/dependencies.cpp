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
#include <optional>
#include <span>
#include <stdexcept>
#include <tuple>
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

namespace {

constexpr std::size_t header_size = record_header_size;
constexpr std::size_t footer_size = record_footer_size;
static_assert(footer_size == 8 + hash_size);

std::uint32_t end_of_unit(const auto& firsts, std::uint32_t u,
    std::size_t total) {
    return u + 1 < firsts.size() ? firsts.values[u + 1].value
                                 : static_cast<std::uint32_t>(total);
}

void encode_unit(std::string& out, std::string& contents,
    const types::dependency_record& record, types::record_unit_id unit) {
    const auto u = unit.value;
    put_text(out, record.codecs[unit]);
    put(out, record.versions[unit]);
    put(out, record.configurations[unit]);
    put_text(out, record.documents[unit]);

    const auto input_end = end_of_unit(record.input_firsts, u,
        record.input_count());
    const auto input_begin = record.input_firsts[unit].value;
    put(out, input_end - input_begin);
    for (auto i = input_begin; i < input_end; ++i) {
        const types::input_id in{i};
        put(out, static_cast<std::uint8_t>(record.input_kinds[in]));
        put_text(out, record.input_keys[in]);
        put(out, record.input_found[in]);
        put(out, record.input_values[in]);
    }

    const auto file_end = end_of_unit(record.file_firsts, u,
        record.files.size());
    const auto file_begin = record.file_firsts[unit].value;
    put(out, file_end - file_begin);
    for (auto f = file_begin; f < file_end; ++f) {
        const types::file_id file{f};
        const auto content = record.files.contents[file];
        put_text(out, record.files.paths[file]);
        put(out, record.file_hashes[file]);
        put(out, static_cast<std::uint64_t>(content.size()));
        contents += content;
    }

    put(out, record.diagnostic_counts[unit]);
    put(out, record.diagnostic_hashes[unit]);
    put_text(out, record.diagnostics[unit]);
    put(out, record.manifests[unit]);
    put(out, record.results[unit]);
}

/**
 * @brief Appends the unit whose head @p r reads to @p record, with the
 * contents of its files taken in order from @p contents and checked
 * against their blob IDs, or left empty without them. Returns the size of
 * each file.
 */
std::vector<std::uint64_t> decode_unit(reader& r,
    types::dependency_record& record,
    std::optional<std::string_view> contents) {
    record.codecs.add(r.text());
    record.versions.values.push_back(r.u32());
    record.configurations.values.push_back(r.id());
    record.documents.add(r.text());

    record.input_firsts.values.push_back(
        {static_cast<std::uint32_t>(record.input_count())});
    const auto inputs = r.u32();
    for (std::uint32_t i = 0; i < inputs; ++i) {
        const auto kind = r.u8();
        if (kind > static_cast<std::uint8_t>(types::last_input_kind))
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
    std::vector<std::uint64_t> sizes;
    std::uint64_t at = 0;
    for (std::uint32_t f = 0; f < files; ++f) {
        const auto path = r.text();
        const auto hash = r.id();
        const auto size = sizes.emplace_back(r.u64());
        std::string_view content;
        if (contents) {
            if (contents->size() - at < size)
                reject("an output runs past its unit's contents");
            content = contents->substr(at, size);
            if (compass::store::ops::blob_id_of(content) != hash)
                reject("an output's blob ID does not match");
            at += size;
        }
        record.file_hashes.values.push_back(hash);
        record.files.add(path, content);
    }
    if (contents && at != contents->size())
        reject("bytes after a unit's contents");

    record.diagnostic_counts.values.push_back(r.u32());
    record.diagnostic_hashes.values.push_back(r.id());
    record.diagnostics.add(r.text());
    record.manifests.values.push_back(r.id());
    record.results.values.push_back(r.id());
    return sizes;
}

/**
 * @brief One link of a unit to a subject, before they are grouped.
 */
struct posting {
    types::subject_kind kind;
    std::string_view key;
    std::uint32_t unit;

    auto operator<=>(const posting&) const = default;
};

std::vector<posting> postings_of(const types::dependency_record& record) {
    using types::input_kind;
    using types::subject_kind;
    std::vector<posting> out;
    for (std::uint32_t u = 0; u < record.size(); ++u) {
        const types::record_unit_id unit{u};
        const auto input_end = end_of_unit(record.input_firsts, u,
            record.input_count());
        for (auto i = record.input_firsts[unit].value; i < input_end; ++i) {
            const types::input_id in{i};
            const auto key = record.input_keys[in];
            switch (record.input_kinds[in]) {
            case input_kind::path:
            case input_kind::file:
            case input_kind::file_exists:
                out.push_back({subject_kind::reads, key, u});
                break;
            case input_kind::id:
                out.push_back({subject_kind::id, key, u});
                break;
            case input_kind::listing:
                out.push_back({subject_kind::listing, key, u});
                break;
            case input_kind::type_set:
                out.push_back({subject_kind::type_set, {}, u});
                break;
            case input_kind::type_registry:
                out.push_back({subject_kind::type_registry, {}, u});
                break;
            }
        }
        const auto file_end = end_of_unit(record.file_firsts, u,
            record.files.size());
        for (auto f = record.file_firsts[unit].value; f < file_end; ++f)
            out.push_back({subject_kind::output,
                record.files.paths[types::file_id{f}], u});
        for (const auto path : diagnostic_paths(record.diagnostics[unit]))
            out.push_back({subject_kind::finding, path, u});
    }
    std::ranges::sort(out);
    const auto [first, last] = std::ranges::unique(out);
    out.erase(first, last);
    return out;
}

void encode_index(std::string& out, const types::dependency_record& record,
    std::span<const std::uint64_t> offsets,
    std::span<const std::uint64_t> content_offsets, std::string_view heads) {
    for (const auto offset : offsets)
        put(out, offset);
    for (std::size_t u = 0; u + 1 < offsets.size(); ++u)
        put(out, sha1(heads.substr(offsets[u] - header_size,
            offsets[u + 1] - offsets[u])));
    for (const auto offset : content_offsets)
        put(out, offset);

    const auto postings = postings_of(record);
    std::uint32_t subjects = 0;
    for (std::size_t i = 0; i < postings.size(); ++i)
        if (i == 0 || postings[i].kind != postings[i - 1].kind ||
            postings[i].key != postings[i - 1].key)
            ++subjects;
    put(out, subjects);
    for (std::size_t i = 0; i < postings.size();) {
        auto end = i;
        while (end < postings.size() && postings[end].kind == postings[i].kind
            && postings[end].key == postings[i].key)
            ++end;
        put(out, static_cast<std::uint8_t>(postings[i].kind));
        put_text(out, postings[i].key);
        put(out, static_cast<std::uint32_t>(end - i));
        for (; i < end; ++i)
            put(out, postings[i].unit);
    }
}

void check_header(std::string_view header) {
    if (header.size() < header_size)
        reject("truncated");
    reader r(header.substr(0, header_size));
    if (r.take(magic.size()) != magic)
        reject("bad magic");
    if (const auto version = r.u32(); version != dependency_record_version)
        reject("version " + std::to_string(version) + " is not " +
            std::to_string(dependency_record_version));
}

}

std::vector<std::string_view> diagnostic_paths(std::string_view bytes) {
    std::vector<std::string_view> out;
    try {
        reader r(bytes);
        while (!r.done()) {
            r.u8();
            out.push_back(r.text());
            r.take(4 + 4 + 4 + 1);
            r.text();
            r.text();
        }
    } catch (const std::runtime_error&) {
        out.clear();
    }
    return out;
}

std::string encode_dependency_record(const types::dependency_record& record) {
    std::string heads;
    std::string contents;
    std::vector<std::uint64_t> offsets;
    std::vector<std::uint64_t> content_offsets;
    offsets.reserve(record.size() + 1);
    content_offsets.reserve(record.size() + 1);
    for (std::uint32_t u = 0; u < record.size(); ++u) {
        offsets.push_back(heads.size());
        content_offsets.push_back(contents.size());
        encode_unit(heads, contents, record, types::record_unit_id{u});
    }
    offsets.push_back(heads.size());
    content_offsets.push_back(contents.size());
    for (auto& o : offsets)
        o += header_size;
    for (auto& o : content_offsets)
        o += header_size + heads.size();

    std::string out(magic);
    put(out, dependency_record_version);
    put(out, static_cast<std::uint32_t>(record.size()));
    out += heads;
    out += contents;
    const auto index_offset = out.size();
    encode_index(out, record, offsets, content_offsets, heads);
    const auto digest = sha1(std::string(out.substr(0, header_size)) +
        out.substr(index_offset));
    put(out, static_cast<std::uint64_t>(index_offset));
    put(out, digest);
    return out;
}

record_layout locate_unit_index(std::string_view header,
    std::string_view footer, std::uint64_t file_size) {
    if (header.size() != header_size || footer.size() != footer_size ||
        file_size < header_size + footer_size)
        reject("truncated");
    check_header(header);
    reader r(footer);
    record_layout l;
    l.index_offset = r.u64();
    l.digest = r.id();
    if (l.index_offset < header_size ||
        l.index_offset > file_size - footer_size)
        reject("the index is out of bounds");
    l.index_size = file_size - footer_size - l.index_offset;
    return l;
}

types::unit_index decode_unit_index(std::string_view header,
    std::string_view section, const record_layout& layout) {
    if (sha1(std::string(header) + std::string(section)) != layout.digest)
        reject("the index checksum does not match");
    reader h(header);
    h.take(magic.size() + 4);
    const auto units = h.u32();

    reader r(section);
    types::unit_index index;
    index.offsets.reserve(units + 1);
    for (std::uint32_t u = 0; u <= units; ++u)
        index.offsets.push_back(r.u64());
    for (std::uint32_t u = 0; u < units; ++u)
        index.hashes.values.push_back(r.id());
    index.content_offsets.reserve(units + 1);
    for (std::uint32_t u = 0; u <= units; ++u)
        index.content_offsets.push_back(r.u64());
    if (index.offsets.front() != header_size ||
        index.offsets.back() != index.content_offsets.front() ||
        index.content_offsets.back() != layout.index_offset ||
        !std::ranges::is_sorted(index.offsets) ||
        !std::ranges::is_sorted(index.content_offsets))
        reject("the unit offsets do not tile the record");
    const auto subjects = r.u32();
    for (std::uint32_t s = 0; s < subjects; ++s) {
        const auto kind = r.u8();
        if (kind > static_cast<std::uint8_t>(types::last_subject_kind))
            reject("unknown subject kind " + std::to_string(kind));
        index.kinds.values.push_back(static_cast<types::subject_kind>(kind));
        index.keys.add(r.text());
        index.unit_firsts.values.push_back(
            static_cast<std::uint32_t>(index.units.size()));
        const auto count = r.u32();
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto unit = r.u32();
            if (unit >= units)
                reject("a subject names unit " + std::to_string(unit));
            index.units.push_back({unit});
        }
    }
    if (!r.done())
        reject("bytes after the last subject");
    return index;
}

std::vector<std::uint64_t> decode_record_unit(std::string_view head,
    const blob_id& hash, types::dependency_record& to,
    std::optional<std::string_view> contents) {
    if (sha1(head) != hash)
        reject("a unit checksum does not match");
    reader r(head);
    auto sizes = decode_unit(r, to, contents);
    if (!r.done())
        reject("bytes after a unit");
    return sizes;
}

types::dependency_record decode_dependency_record(std::string_view bytes) {
    if (bytes.size() < header_size + footer_size)
        reject("truncated");
    check_header(bytes);
    const auto header = bytes.substr(0, header_size);
    const auto layout = locate_unit_index(header,
        bytes.substr(bytes.size() - footer_size), bytes.size());
    const auto index = decode_unit_index(header,
        bytes.substr(layout.index_offset, layout.index_size), layout);
    types::dependency_record record;
    for (std::uint32_t u = 0; u < index.size(); ++u)
        decode_record_unit(bytes.substr(index.offsets[u],
            index.offsets[u + 1] - index.offsets[u]),
            index.hashes.values[u], record,
            bytes.substr(index.content_offsets[u],
                index.content_offsets[u + 1] - index.content_offsets[u]));
    return record;
}

std::span<const types::record_unit_id> units_of(const types::unit_index& index,
    types::subject_kind kind, std::string_view key) {
    std::uint32_t lo = 0;
    auto hi = static_cast<std::uint32_t>(index.subject_count());
    while (lo < hi) {
        const auto mid = lo + (hi - lo) / 2;
        const types::subject_id s{mid};
        if (std::tuple(index.kinds[s], index.keys[s]) < std::tuple(kind, key))
            lo = mid + 1;
        else
            hi = mid;
    }
    const types::subject_id s{lo};
    if (lo == index.subject_count() || index.kinds[s] != kind ||
        index.keys[s] != key)
        return {};
    const auto first = index.unit_firsts[s];
    const auto end = lo + 1 < index.subject_count()
        ? index.unit_firsts[s.next()]
        : static_cast<std::uint32_t>(index.units.size());
    return std::span(index.units).subspan(first, end - first);
}

std::vector<types::record_unit_id> units_touching(
    const types::unit_index& index, const edited_file& file) {
    using types::subject_kind;
    std::vector<types::record_unit_id> out;
    const auto add = [&](subject_kind kind, std::string_view key) {
        const auto units = units_of(index, kind, key);
        out.insert(out.end(), units.begin(), units.end());
    };
    const auto path = file.path;
    add(subject_kind::reads, path);
    add(subject_kind::finding, path);
    if (file.output)
        add(subject_kind::output, *file.output);
    if (file.document) {
        add(subject_kind::type_set, {});
        for (const auto id : file.ids)
            add(subject_kind::id, id);
    }
    const auto& kinds = index.kinds.values;
    const auto first = file.created
        ? std::ranges::lower_bound(kinds, subject_kind::listing) : kinds.end();
    for (auto it = first; it != kinds.end() && *it == subject_kind::listing;
         ++it) {
        const types::subject_id s{
            static_cast<std::uint32_t>(it - kinds.begin())};
        const auto pattern = index.keys[s];
        const auto star = pattern.find('*');
        const auto prefix = pattern.substr(0, star);
        const auto suffix = star == std::string_view::npos
            ? std::string_view{} : pattern.substr(star + 1);
        if (path.size() >= prefix.size() + suffix.size() &&
            path.starts_with(prefix) && path.ends_with(suffix))
            add(subject_kind::listing, pattern);
    }
    std::ranges::sort(out);
    const auto [from, to] = std::ranges::unique(out);
    out.erase(from, to);
    return out;
}

}

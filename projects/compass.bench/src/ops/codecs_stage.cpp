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
#include "compass.bench/ops/codecs_stage.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include "compass.bench/ops/partition.hpp"
#include "compass.bench/ops/types_stage.hpp"
#include "compass.codec/codecs/catalogue.hpp"
#include "compass.codec/io/dependency_file.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/corpus.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/io/object.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.store/io/id_resolver.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/io/object_file.hpp"

namespace compass::bench::ops {

namespace {

using platform::filesystem::mapped_file;
namespace ot = compass::org::types;
namespace ct = compass::codec::types;

constexpr std::size_t top_count = 15;
constexpr std::size_t first_count = 10;

/**
 * @brief The org files of the checkout, one row each, with the mappings
 * the corpus points into. ok is 0 for a row that did not load.
 */
struct loaded_files {
    std::vector<std::string_view> paths;
    std::vector<std::uint64_t> sizes;
    std::vector<store::io::resolved_id> resolved;
    std::vector<std::optional<mapped_file>> sources;
    std::vector<std::optional<mapped_file>> objects;
    std::vector<ot::tables_view> tables;
    std::vector<ot::type_id> types;
    std::vector<std::uint8_t> ok;
};

struct tally {
    std::uint64_t loaded = 0;
    std::uint64_t written = 0;
    std::uint64_t errors = 0;
    std::string first_error;
};

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

/**
 * @brief Maps the object at @p path into row @p row and returns its
 * tables, or nothing when it is missing, does not decode or fails its
 * checksum.
 */
std::optional<ot::tables_view> map_object(loaded_files& f, std::size_t row,
    const std::filesystem::path& path,
    const compass::org::io::object_key& key) {
    try {
        f.objects[row].emplace(path);
    } catch (const std::system_error&) {
        return std::nullopt;
    }
    const auto bytes = f.objects[row]->bytes();
    const auto view = compass::org::io::decode(bytes, key);
    if (!view || !compass::org::io::checksum_matches(bytes))
        return std::nullopt;
    return *view;
}

tally load_range(loaded_files& f, const std::filesystem::path& checkout,
    const std::filesystem::path& objects,
    const store::io::id_resolver& resolver, const ot::type_registry& types,
    types::file_range range) {
    tally t;
    ot::tables parsed;
    std::vector<std::byte> bytes;
    for (std::size_t row = range.begin; row < range.end; ++row) {
        try {
            f.resolved[row] = resolver.resolve(f.paths[row]);
            const auto& source = f.sources[row].emplace(
                checkout / f.paths[row]);
            const compass::org::io::object_key key{
                f.resolved[row].id.bytes, ingest::org::parser_version};
            const auto path =
                store::io::object_path(objects, f.resolved[row].id);
            auto view = map_object(f, row, path, key);
            if (view) {
                ++t.loaded;
            } else {
                ingest::org::parse(source.text(), parsed);
                compass::org::io::encode(ot::view(parsed), key, bytes);
                store::io::write_object(path, bytes);
                view = map_object(f, row, path, key);
                if (!view)
                    throw std::system_error(
                        std::make_error_code(std::errc::io_error),
                        "the object just written does not load");
                ++t.written;
            }
            f.tables[row] = *view;
            f.types[row] =
                codec::ops::document_type(source.text(), *view, types);
            f.ok[row] = 1;
        } catch (const std::system_error& e) {
            if (t.errors++ == 0)
                t.first_error = std::string(f.paths[row]) + ": " + e.what();
        }
    }
    return t;
}

using text_counts = std::map<std::string, std::uint64_t, std::less<>>;

std::vector<types::counted_text> top(const text_counts& counts) {
    std::vector<types::counted_text> out;
    for (const auto& [text, count] : counts)
        out.push_back({text, count});
    std::ranges::stable_sort(out, std::greater{},
        &types::counted_text::count);
    if (out.size() > top_count)
        out.resize(top_count);
    return out;
}

std::size_t line_of(std::string_view source, std::uint32_t offset) {
    return 1 + static_cast<std::size_t>(
        std::ranges::count(source.substr(0, offset), '\n'));
}

types::findings_summary summarise(const ct::corpus& c,
    const ct::descriptor& d, const ct::diagnostic_table& t) {
    types::findings_summary s;
    text_counts rules;
    text_counts messages;
    auto previous = ot::document_id::none();
    for (std::uint32_t i = 0; i < t.size(); ++i) {
        const ct::diagnostic_id id{i};
        const auto document = t.documents[id];
        if (document != previous)
            ++s.documents;
        previous = document;
        const auto rule = d.rules[t.rules[id].value].name;
        ++rules[std::string(rule)];
        ++messages[std::string(t.messages[id])];
        if (s.first.size() < first_count)
            s.first.push_back(std::format("{}:{}: {}: {}/{}: {}. {}",
                c.paths[document.value],
                line_of(c.sources[document.value], t.ranges[id].begin),
                t.levels[id] == ct::severity::error ? "error" : "warning",
                d.name, rule, t.messages[id], t.fixes[id]));
    }
    s.rules = top(rules);
    s.messages = top(messages);
    return s;
}

}

types::codecs_result run_codecs_stage(const std::filesystem::path& checkout,
    const types::file_table& table, const std::filesystem::path& meta,
    const std::filesystem::path& objects,
    const codec::types::configuration& settings, unsigned threads,
    unsigned repetitions) {
    types::codecs_result r;

    auto start = std::chrono::steady_clock::now();
    const auto registry = build_meta_registry(meta).registry;
    r.registry_ms = since(start);
    r.registered_types = registry.types.size();

    loaded_files f;
    for (std::uint32_t id = 0; id < table.size(); ++id) {
        if (table.extensions[table.extension_ids[id]] == ".org") {
            f.paths.emplace_back(table.paths[id]);
            f.sizes.push_back(table.sizes[id]);
        }
    }
    const auto count = f.paths.size();
    f.resolved.resize(count);
    f.sources.resize(count);
    f.objects.resize(count);
    f.tables.resize(count);
    f.types.resize(count);
    f.ok.assign(count, 0);
    r.files = count;

    start = std::chrono::steady_clock::now();
    const store::io::id_resolver resolver(checkout);
    const auto tallies = run_ranges(balance_ranges(f.sizes, threads),
        [&](types::file_range range) {
            return load_range(f, checkout, objects, resolver, registry,
                range);
        }).second;
    resolver.write_worktree_index(f.paths, f.resolved);
    r.load_ms = since(start);
    for (const auto& t : tallies) {
        r.loaded += t.loaded;
        r.written += t.written;
        r.errors += t.errors;
        if (r.first_error.empty())
            r.first_error = t.first_error;
    }

    ct::corpus corpus;
    for (std::size_t row = 0; row < count; ++row) {
        if (f.ok[row] == 0)
            continue;
        corpus.paths.push_back(f.paths[row]);
        corpus.contents.push_back(f.resolved[row].id);
        corpus.sources.push_back(f.sources[row]->text());
        corpus.tables.push_back(f.tables[row]);
        corpus.types.push_back(f.types[row]);
    }
    r.documents = corpus.size();

    r.registry = codec::ops::make_registry(codec::codecs::catalogue(),
        registry, settings);
    start = std::chrono::steady_clock::now();
    const auto record_path = codec::io::dependency_file_path(
        store::io::git_dir(checkout));
    const auto previous = codec::io::read_dependency_file(record_path);
    r.record_read_ms = since(start);
    r.record_problem = previous.problem;
    r.record_bytes = previous.bytes;
    r.record_units = previous.record.size();

    r.ms.resize(r.registry.size());
    for (unsigned rep = 0; rep < repetitions; ++rep) {
        r.last = codec::ops::run(r.registry, corpus, registry, threads,
            previous.record);
        r.wall_ms.push_back(r.last.wall_ms);
        for (std::size_t c = 0; c < r.registry.size(); ++c)
            r.ms[c].push_back(r.last.ms[c]);
    }
    if (repetitions != 0) {
        start = std::chrono::steady_clock::now();
        r.record_written = codec::io::write_dependency_file(record_path,
            r.last.record);
        r.record_write_ms = since(start);
    }
    for (std::size_t c = 0; c < r.registry.size(); ++c)
        r.findings.push_back(summarise(corpus, r.registry.codecs[c],
            r.last.diagnostics[c]));
    return r;
}

}

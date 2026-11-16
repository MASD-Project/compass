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
#include "compass.cli/app/pipeline.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <vector>
#include "compass.cli/config/repository_file.hpp"
#include "compass.cli/io/listing.hpp"
#include "compass.codec/codecs/catalogue.hpp"
#include "compass.codec/io/action_cache.hpp"
#include "compass.codec/io/dependency_file.hpp"
#include "compass.codec/io/output_sink.hpp"
#include "compass.codec/ops/outputs.hpp"
#include "compass.codec/ops/parallel_for.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/corpus.hpp"
#include "compass.ingest/meta/type_registry_builder.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/io/object.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.store/io/collection.hpp"
#include "compass.store/io/id_resolver.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/io/object_file.hpp"

namespace compass::cli::app {

namespace {

using platform::filesystem::mapped_file;
namespace ot = compass::org::types;
namespace ct = compass::codec::types;

constexpr std::size_t files_per_task = 32;

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

/**
 * @brief The type registry the contract pages document_type_*.org under
 * @p meta declare, read in name order, with the model types after them.
 */
ot::type_registry build_type_registry(const std::filesystem::path& meta) {
    std::vector<std::filesystem::path> pages;
    for (const auto& entry : std::filesystem::directory_iterator(meta)) {
        const auto name = entry.path().filename().string();
        if (entry.is_regular_file() && name.starts_with("document_type_") &&
            name.ends_with(".org"))
            pages.push_back(entry.path());
    }
    std::ranges::sort(pages);

    ingest::meta::type_registry_builder builder;
    ot::tables tables;
    for (const auto& page : pages) {
        const mapped_file file(page);
        ingest::org::parse(file.text(), tables);
        std::ignore = builder.add_contract(file.text(), ot::view(tables));
    }
    for (const auto name : ingest::meta::model_types)
        builder.add_model(name);
    return builder.build();
}

/**
 * @brief Everything a run of the pipeline read and computed, before
 * anything is written, one row per org file of the checkout.
 *
 * The corpus points into sources and objects, which stay mapped while the
 * pipeline object lives. encoded holds the object of a row that was
 * parsed, stored only when the run persists, and is empty for a row
 * mapped from the store. error is empty for a row that loaded.
 */
struct pipeline {
    std::filesystem::path checkout;
    std::filesystem::path objects;
    std::vector<std::string> paths;
    std::vector<std::string_view> path_views;
    std::vector<store::io::resolved_id> resolved;
    std::vector<std::optional<mapped_file>> sources;
    std::vector<std::optional<mapped_file>> mapped;
    std::vector<std::vector<std::byte>> encoded;
    std::vector<ot::tables_view> tables;
    std::vector<ot::type_id> types;
    std::vector<std::string> errors;
    std::optional<store::io::id_resolver> resolver;
    ot::type_registry type_registry;
    ct::corpus corpus;
    ct::registry codecs;
    ct::run_result run;
    types::run_summary summary;
};

/**
 * @brief The tables of the object at @p path, mapped into @p slot, or
 * nothing when it is missing, does not decode or fails its checksum.
 */
std::optional<ot::tables_view> map_object(std::optional<mapped_file>& slot,
    const std::filesystem::path& path,
    const compass::org::io::object_key& key) {
    try {
        slot.emplace(path);
    } catch (const std::system_error&) {
        return std::nullopt;
    }
    const auto bytes = slot->bytes();
    const auto view = compass::org::io::decode(bytes, key);
    if (!view || !compass::org::io::checksum_matches(bytes)) {
        slot.reset();
        return std::nullopt;
    }
    return *view;
}

/**
 * @brief Loads row @p row of @p p: maps its object from the store or,
 * when it is missing or rejected, parses the file into @p scratch and
 * keeps the encoded object in memory. Returns true when it was mapped.
 */
bool load_row(pipeline& p, std::size_t row, ot::tables& scratch) {
    p.resolved[row] = p.resolver->resolve(p.path_views[row]);
    const auto& source = p.sources[row].emplace(p.checkout / p.paths[row]);
    const compass::org::io::object_key key{
        p.resolved[row].id.bytes, ingest::org::parser_version};
    auto view = map_object(p.mapped[row],
        store::io::object_path(p.objects, p.resolved[row].id), key);
    const bool from_store = view.has_value();
    if (!from_store) {
        ingest::org::parse(source.text(), scratch);
        auto& bytes = p.encoded[row];
        compass::org::io::encode(ot::view(scratch), key, bytes);
        const auto decoded = compass::org::io::decode(bytes, key);
        if (!decoded || !compass::org::io::checksum_matches(bytes))
            throw std::system_error(
                std::make_error_code(std::errc::io_error),
                "the object just encoded does not load");
        view = *decoded;
    }
    p.tables[row] = *view;
    p.types[row] =
        codec::ops::document_type(source.text(), *view, p.type_registry);
    return from_store;
}

/**
 * @brief Lists, loads and runs the codecs over the checkout of @p o into
 * @p p. Reads the checkout, the store, the dependency record and the
 * result cache, and writes nothing.
 */
void evaluate(pipeline& p, const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    auto& s = p.summary;
    p.checkout = o.checkout;
    p.objects = store::io::objects_dir(store::io::git_common_dir(o.checkout));
    p.type_registry = build_type_registry(o.meta);

    p.paths = io::list_org_files(o.checkout);
    std::erase(p.paths, config::repository_file_name);
    const auto count = p.paths.size();
    p.path_views.assign(p.paths.begin(), p.paths.end());
    p.resolved.resize(count);
    p.sources.resize(count);
    p.mapped.resize(count);
    p.encoded.resize(count);
    p.tables.resize(count);
    p.types.resize(count);
    p.errors.resize(count);
    std::vector<std::uint8_t> from_store(count, 0);
    s.files = count;

    p.resolver.emplace(o.checkout);
    const auto tasks = (count + files_per_task - 1) / files_per_task;
    codec::ops::parallel_for(tasks, o.threads, [&](std::size_t task) {
        ot::tables scratch;
        const auto end = std::min(count, (task + 1) * files_per_task);
        for (auto row = task * files_per_task; row < end; ++row) {
            try {
                from_store[row] = load_row(p, row, scratch) ? 1 : 0;
            } catch (const std::system_error& e) {
                p.errors[row] = e.what();
            }
        }
    });

    auto& corpus = p.corpus;
    for (std::size_t row = 0; row < count; ++row) {
        if (!p.errors[row].empty()) {
            s.errors.push_back(p.paths[row] + ": " + p.errors[row]);
            continue;
        }
        ++(from_store[row] != 0 ? s.loaded : s.parsed);
        corpus.paths.push_back(p.path_views[row]);
        corpus.contents.push_back(p.resolved[row].id);
        corpus.sources.push_back(p.sources[row]->text());
        corpus.tables.push_back(p.tables[row]);
        corpus.types.push_back(p.types[row]);
    }
    s.documents = corpus.size();

    p.codecs = codec::ops::make_registry(codec::codecs::catalogue(),
        p.type_registry, o.settings);
    s.problems = p.codecs.problems;
    const auto previous = codec::io::read_dependency_file(
        codec::io::dependency_file_path(store::io::git_dir(o.checkout)));
    p.run = codec::ops::run(p.codecs, corpus, p.type_registry, o.threads,
        previous.record,
        codec::io::action_source_of(store::io::git_common_dir(o.checkout)));
    for (std::size_t c = 0; c < p.codecs.size(); ++c)
        s.codecs.emplace_back(p.codecs.codecs[c].name);
    s.ran = p.run.ran;
    s.ms = since(start);
}

/**
 * @brief Writes what @p p computed that later runs reuse: the objects it
 * parsed, the worktree index, the result cache and the dependency record.
 */
void persist(const pipeline& p, unsigned threads) {
    for (std::size_t row = 0; row < p.paths.size(); ++row) {
        if (p.errors[row].empty() && !p.encoded[row].empty())
            store::io::write_object(
                store::io::object_path(p.objects, p.resolved[row].id),
                p.encoded[row]);
    }
    std::vector<std::string_view> paths;
    std::vector<store::io::resolved_id> resolved;
    for (std::size_t row = 0; row < p.paths.size(); ++row) {
        if (p.errors[row].empty()) {
            paths.push_back(p.path_views[row]);
            resolved.push_back(p.resolved[row]);
        }
    }
    p.resolver->write_worktree_index(paths, resolved);
    std::ignore = codec::io::store_actions(
        store::io::git_common_dir(p.checkout), p.run.record, p.run.fresh,
        threads);
    codec::io::write_dependency_file(
        codec::io::dependency_file_path(store::io::git_dir(p.checkout)),
        p.run.record);
}

/**
 * @brief The 1-based line and byte column of @p offset in @p source.
 */
std::pair<std::uint32_t, std::uint32_t> position_of(std::string_view source,
    std::uint32_t offset) {
    const auto before = source.substr(0, offset);
    const auto line = 1 + std::ranges::count(before, '\n');
    const auto newline = before.rfind('\n');
    const auto column = newline == std::string_view::npos
        ? before.size() + 1 : before.size() - newline;
    return {static_cast<std::uint32_t>(line),
        static_cast<std::uint32_t>(column)};
}

}

types::generate_result generate(const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    types::generate_result r;
    r.root = o.output_root;
    pipeline p;
    evaluate(p, o);
    persist(p, o.threads);
    r.run = p.summary;
    if (!r.run.failed()) {
        const auto record_path = codec::io::written_outputs_path(
            store::io::git_dir(o.checkout));
        const auto previous = codec::io::read_written_outputs(record_path);
        r.sink = codec::io::write_outputs(o.output_root,
            codec::ops::outputs_of(p.codecs, p.run), previous.record);
        if (r.sink.problems.empty())
            codec::io::write_written_outputs(record_path, r.sink.record);
    }
    r.ms = since(start);
    return r;
}

types::diff_result diff(const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    types::diff_result r;
    r.root = o.output_root;
    pipeline p;
    evaluate(p, o);
    r.run = p.summary;
    if (!r.run.failed()) {
        const auto previous = codec::io::read_written_outputs(
            codec::io::written_outputs_path(store::io::git_dir(o.checkout)));
        const auto outputs = codec::ops::outputs_of(p.codecs, p.run);
        const auto plan = codec::io::plan_outputs(o.output_root, outputs,
            previous.record);
        r.planned = codec::io::summarise(plan);
        r.diff = codec::io::diff_outputs(plan);
    }
    r.ms = since(start);
    return r;
}

types::check_result check(const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    types::check_result r;
    pipeline p;
    evaluate(p, o);
    persist(p, o.threads);
    r.run = p.summary;

    for (std::size_t c = 0; c < p.codecs.size(); ++c) {
        const auto& d = p.codecs.codecs[c];
        const auto& t = p.run.diagnostics[c];
        for (std::uint32_t i = 0; i < t.size(); ++i) {
            const ct::diagnostic_id id{i};
            const auto document = t.documents[id].value;
            const auto [line, column] =
                position_of(p.corpus.sources[document], t.ranges[id].begin);
            r.findings.push_back({
                .path = std::string(p.corpus.paths[document]),
                .line = line,
                .column = column,
                .rule = std::string(d.name) + "/" +
                    std::string(d.rules[t.rules[id].value].name),
                .level = t.levels[id],
                .message = std::string(t.messages[id]),
                .fix = std::string(t.fixes[id])});
        }
    }
    std::ranges::stable_sort(r.findings, {}, [](const types::finding& f) {
        return std::tie(f.path, f.line, f.column);
    });
    r.ms = since(start);
    return r;
}

types::gc_result gc(const types::options& o) {
    types::gc_result r;
    r.objects = store::io::objects_dir(store::io::git_common_dir(o.checkout));
    r.options = o.collection;
    r.collection = store::io::collect(o.checkout, r.objects, o.collection,
        codec::io::action_keys);
    return r;
}

}

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
#include "compass.cli/app/steps.hpp"

#include <algorithm>
#include <system_error>
#include <tuple>
#include <utility>
#include "compass.cli/config/repository_file.hpp"
#include "compass.cli/io/listing.hpp"
#include "compass.codec/ops/outputs.hpp"
#include "compass.codec/ops/parallel_for.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.ingest/meta/type_registry_builder.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/io/object.hpp"
#include "compass.store/io/collection.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/io/object_file.hpp"

namespace compass::cli::app {

namespace {

using platform::filesystem::mapped_file;
namespace ot = compass::org::types;

constexpr std::size_t files_per_task = 32;

/**
 * @brief Calls @p row_of for every row of @p f in tasks of files_per_task
 * rows on up to @p threads threads, recording what a row throws as its
 * error.
 */
template <typename Row>
void for_each_row(corpus_files& f, unsigned threads, Row row_of) {
    const auto count = f.paths.size();
    const auto tasks = (count + files_per_task - 1) / files_per_task;
    codec::ops::parallel_for(tasks, threads, [&](std::size_t task) {
        ot::tables scratch;
        const auto end = std::min(count, (task + 1) * files_per_task);
        for (auto row = task * files_per_task; row < end; ++row) {
            try {
                row_of(row, scratch);
            } catch (const std::system_error& e) {
                f.errors[row] = e.what();
            }
        }
    });
}

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

void load_row(corpus_files& f, std::size_t row, ot::tables& scratch) {
    const auto& source = f.sources[row].emplace(f.checkout / f.paths[row]);
    const compass::org::io::object_key key{
        f.resolved[row].id.bytes, ingest::org::parser_version};
    auto view = map_object(f.mapped[row],
        store::io::object_path(f.objects, f.resolved[row].id), key);
    if (!view) {
        ingest::org::parse(source.text(), scratch);
        auto& bytes = f.encoded[row];
        compass::org::io::encode(ot::view(scratch), key, bytes);
        const auto decoded = compass::org::io::decode(bytes, key);
        if (!decoded || !compass::org::io::checksum_matches(bytes))
            throw std::system_error(
                std::make_error_code(std::errc::io_error),
                "the object just encoded does not load");
        view = *decoded;
    }
    f.tables[row] = *view;
}

}

std::vector<std::string> list(const std::filesystem::path& checkout) {
    auto paths = io::list_org_files(checkout);
    std::erase(paths, config::repository_file_name);
    return paths;
}

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

corpus_files open_files(const std::filesystem::path& checkout,
    const std::filesystem::path& objects,
    std::vector<std::string_view> paths) {
    corpus_files f;
    f.checkout = checkout;
    f.objects = objects;
    f.paths = std::move(paths);
    const auto count = f.paths.size();
    f.resolved.resize(count);
    f.sources.resize(count);
    f.mapped.resize(count);
    f.encoded.resize(count);
    f.tables.resize(count);
    f.errors.resize(count);
    return f;
}

void resolve(corpus_files& f, unsigned threads) {
    const auto& resolver = f.resolver.emplace(f.checkout);
    for_each_row(f, threads, [&](std::size_t row, ot::tables&) {
        f.resolved[row] = resolver.resolve(f.paths[row]);
    });
}

void load(corpus_files& f, unsigned threads) {
    for_each_row(f, threads, [&](std::size_t row, ot::tables& scratch) {
        if (f.errors[row].empty())
            load_row(f, row, scratch);
    });
}

codec::types::corpus corpus_of(const corpus_files& f,
    const ot::type_registry& types) {
    codec::types::corpus corpus;
    for (std::size_t row = 0; row < f.paths.size(); ++row) {
        if (!f.errors[row].empty())
            continue;
        const auto source = f.sources[row]->text();
        corpus.paths.push_back(f.paths[row]);
        corpus.contents.push_back(f.resolved[row].id);
        corpus.sources.push_back(source);
        corpus.tables.push_back(f.tables[row]);
        corpus.types.push_back(
            codec::ops::document_type(source, f.tables[row], types));
    }
    return corpus;
}

codec::io::loaded_record read_dependency_record(
    const std::filesystem::path& checkout) {
    return codec::io::read_dependency_file(
        codec::io::dependency_file_path(store::io::git_dir(checkout)));
}

codec::types::run_result run_codecs(const codec::types::registry& codecs,
    const codec::types::corpus& corpus, const ot::type_registry& types,
    unsigned threads, const codec::types::dependency_record& previous,
    const std::filesystem::path& checkout) {
    return codec::ops::run(codecs, corpus, types, threads, previous,
        codec::io::action_source_of(store::io::git_common_dir(checkout)));
}

std::uint64_t store_objects(const corpus_files& f) {
    std::uint64_t written = 0;
    for (std::size_t row = 0; row < f.paths.size(); ++row) {
        if (!f.errors[row].empty() || f.encoded[row].empty())
            continue;
        store::io::write_object(
            store::io::object_path(f.objects, f.resolved[row].id),
            f.encoded[row]);
        ++written;
    }
    return written;
}

std::size_t write_worktree_index(const corpus_files& f) {
    std::vector<std::string_view> paths;
    std::vector<store::io::resolved_id> resolved;
    for (std::size_t row = 0; row < f.paths.size(); ++row) {
        if (f.errors[row].empty()) {
            paths.push_back(f.paths[row]);
            resolved.push_back(f.resolved[row]);
        }
    }
    return f.resolver->write_worktree_index(paths, resolved);
}

codec::io::action_writes store_results(const std::filesystem::path& checkout,
    const codec::types::run_result& run, unsigned threads) {
    return codec::io::store_actions(store::io::git_common_dir(checkout),
        run.record, run.fresh, threads);
}

bool write_dependency_record(const std::filesystem::path& checkout,
    const codec::types::run_result& run) {
    return codec::io::write_dependency_file(
        codec::io::dependency_file_path(store::io::git_dir(checkout)),
        run.record);
}

sink_outcome write_outputs(const std::filesystem::path& checkout,
    const std::filesystem::path& root, const codec::types::registry& codecs,
    const codec::types::run_result& run) {
    sink_outcome r;
    const auto record_path =
        codec::io::written_outputs_path(store::io::git_dir(checkout));
    const auto previous = codec::io::read_written_outputs(record_path);
    r.record_problem = previous.problem;
    r.sink = codec::io::write_outputs(root, codec::ops::outputs_of(codecs, run),
        previous.record);
    if (r.sink.problems.empty())
        r.record_written =
            codec::io::write_written_outputs(record_path, r.sink.record);
    return r;
}

void plan_outputs(planned_outputs& p, const std::filesystem::path& checkout,
    const std::filesystem::path& root, const codec::types::registry& codecs,
    const codec::types::run_result& run) {
    p.previous = codec::io::read_written_outputs(
        codec::io::written_outputs_path(store::io::git_dir(checkout)));
    p.outputs = codec::ops::outputs_of(codecs, run);
    p.plan = codec::io::plan_outputs(root, p.outputs, p.previous.record);
}

store::types::collection_result collect(const std::filesystem::path& checkout,
    const std::filesystem::path& objects,
    const store::types::collection_options& options) {
    return store::io::collect(checkout, objects, options,
        codec::io::action_keys);
}

}

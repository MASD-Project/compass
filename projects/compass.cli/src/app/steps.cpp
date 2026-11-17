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
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
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
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/ops/git_index.hpp"

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

/**
 * @brief The bytes of row @p row: its file mapped from the checkout, or its
 * blob read from the commit.
 */
std::string_view text_of(corpus_files& f, std::size_t row) {
    if (f.commit == nullptr)
        return f.sources[row].emplace(f.checkout / f.paths[row]).text();
    auto blob = f.commit->objects.read(f.resolved[row].id);
    if (!blob)
        throw std::system_error(
            std::make_error_code(std::errc::no_such_file_or_directory),
            "the blob " + store::ops::to_hex(f.resolved[row].id) +
                " is missing from git's object database");
    return f.blobs[row] = std::move(blob->bytes);
}

void load_row(corpus_files& f, std::size_t row, ot::tables& scratch) {
    const auto text = f.texts[row] = text_of(f, row);
    const compass::org::io::object_key key{
        f.resolved[row].id.bytes, ingest::org::parser_version};
    auto view = map_object(f.mapped[row],
        store::io::object_path(f.objects, f.resolved[row].id), key);
    if (!view) {
        ingest::org::parse(text, scratch);
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

/**
 * @brief The type registry @p pages declare, in the order given, with the
 * model types after them.
 */
ot::type_registry registry_of(std::span<const std::string_view> pages) {
    ingest::meta::type_registry_builder builder;
    ot::tables tables;
    for (const auto page : pages) {
        ingest::org::parse(page, tables);
        std::ignore = builder.add_contract(page, ot::view(tables));
    }
    for (const auto name : ingest::meta::model_types)
        builder.add_model(name);
    return builder.build();
}

bool is_contract_page(std::string_view name) noexcept {
    return name.starts_with("document_type_") && name.ends_with(".org");
}

/**
 * @brief The files of @p l as the commit @p c holds them: an ID and bytes
 * for each file and symbolic link, none for a submodule.
 */
codec::types::checkout_files commit_files_of(const commit_source& c,
    const listing& l) {
    codec::types::checkout_files out;
    out.paths.assign(l.files.begin(), l.files.end());
    const auto blob_of = [&c](std::string_view path)
        -> std::optional<store::types::object_id> {
        const auto row = c.files.find(path);
        if (!row || c.files.modes[*row] == store::types::gitlink_mode)
            return std::nullopt;
        return c.files.ids[*row];
    };
    out.content_of = blob_of;
    out.read = [&c, blob_of](std::string_view path)
        -> std::optional<std::string> {
        const auto id = blob_of(path);
        if (!id)
            return std::nullopt;
        auto blob = c.objects.read(*id);
        if (!blob)
            return std::nullopt;
        return std::move(blob->bytes);
    };
    return out;
}

}

commit_source::commit_source(const std::filesystem::path& git_common_dir,
    const store::types::object_id& commit)
    : objects(store::io::git_objects_dir(git_common_dir)), id(commit),
      files(store::io::list_commit(objects, commit)) {}

void list(listing& l, const std::filesystem::path& checkout) {
    l.files = io::list_files(checkout);
    l.documents = io::org_files(checkout, l.files);
    std::erase(l.documents, config::repository_file_name);
}

void list(listing& l, const commit_source& c) {
    l.files = c.files.paths;
    for (std::size_t row = 0; row < c.files.size(); ++row) {
        const std::string_view path = l.files[row];
        if (path.ends_with(".org") &&
            store::ops::is_regular_file(c.files.modes[row]) &&
            path != config::repository_file_name)
            l.documents.push_back(path);
    }
}

codec::types::checkout_files checkout_files_of(const corpus_files& f,
    const listing& l) {
    if (f.commit != nullptr)
        return commit_files_of(*f.commit, l);
    codec::types::checkout_files out;
    out.paths.assign(l.files.begin(), l.files.end());
    const auto resolve = [&f](std::string_view path)
        -> std::optional<store::types::blob_id> {
        try {
            return f.resolver->resolve(path).id;
        } catch (const std::system_error&) {
            return std::nullopt;
        }
    };
    // Many codecs ask for the ID of one file, each lookup an lstat or a
    // hash, so each listed file is resolved once per run.
    struct resolved {
        explicit resolved(std::vector<std::string_view> p)
            : paths(std::move(p)), once(paths.size()), ids(paths.size()) {}
        std::vector<std::string_view> paths;
        std::vector<std::once_flag> once;
        std::vector<std::optional<store::types::blob_id>> ids;
    };
    auto memo = std::make_shared<resolved>(out.paths);
    out.content_of = [memo, resolve](std::string_view path)
        -> std::optional<store::types::blob_id> {
        const auto it = std::ranges::lower_bound(memo->paths, path);
        if (it == memo->paths.end() || *it != path)
            return resolve(path);
        const auto row = static_cast<std::size_t>(it - memo->paths.begin());
        std::call_once(memo->once[row],
            [&] { memo->ids[row] = resolve(path); });
        return memo->ids[row];
    };
    out.read = [&f](std::string_view path) -> std::optional<std::string> {
        try {
            const mapped_file file(f.checkout / path);
            return std::string(file.text());
        } catch (const std::system_error&) {
            return std::nullopt;
        }
    };
    return out;
}

ot::type_registry build_type_registry(const std::filesystem::path& meta) {
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(meta)) {
        if (entry.is_regular_file() &&
            is_contract_page(entry.path().filename().string()))
            paths.push_back(entry.path());
    }
    std::ranges::sort(paths);
    std::vector<mapped_file> files;
    std::vector<std::string_view> pages;
    for (const auto& path : paths)
        pages.push_back(files.emplace_back(path).text());
    return registry_of(pages);
}

ot::type_registry build_type_registry(const commit_source& c,
    const std::filesystem::path& meta) {
    auto prefix = meta.generic_string();
    if (!prefix.empty() && !prefix.ends_with('/'))
        prefix += '/';
    std::vector<std::string> blobs;
    bool directory = prefix.empty();
    for (std::size_t row = 0; row < c.files.size(); ++row) {
        const std::string_view path = c.files.paths[row];
        if (!path.starts_with(prefix))
            continue;
        directory = true;
        if (!store::ops::is_regular_file(c.files.modes[row]))
            continue;
        const auto name = path.substr(prefix.size());
        if (name.find('/') != std::string_view::npos || !is_contract_page(name))
            continue;
        auto blob = c.objects.read(c.files.ids[row]);
        if (!blob)
            throw std::runtime_error("the blob of " + std::string(path) +
                " is missing from git's object database");
        blobs.push_back(std::move(blob->bytes));
    }
    if (!directory)
        throw std::runtime_error(meta.generic_string() +
            " is not a directory of the commit " +
            store::ops::to_hex(c.id));
    const std::vector<std::string_view> pages(blobs.begin(), blobs.end());
    return registry_of(pages);
}

corpus_files open_files(const std::filesystem::path& checkout,
    const std::filesystem::path& objects,
    std::vector<std::string_view> paths, const commit_source* commit) {
    corpus_files f;
    f.checkout = checkout;
    f.objects = objects;
    f.commit = commit;
    f.paths = std::move(paths);
    const auto count = f.paths.size();
    f.resolved.resize(count);
    f.sources.resize(count);
    f.blobs.resize(count);
    f.texts.resize(count);
    f.mapped.resize(count);
    f.encoded.resize(count);
    f.tables.resize(count);
    f.errors.resize(count);
    return f;
}

void resolve(corpus_files& f, unsigned threads) {
    if (f.commit != nullptr) {
        const auto& files = f.commit->files;
        for (std::size_t row = 0; row < f.paths.size(); ++row) {
            if (const auto at = files.find(f.paths[row]))
                f.resolved[row].id = files.ids[*at];
            else
                f.errors[row] = "the commit holds no such file";
        }
        return;
    }
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
        const auto source = f.texts[row];
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
    const std::filesystem::path& checkout,
    const codec::ops::unit_filter& only) {
    return codec::ops::run(codecs, corpus, types, threads, previous,
        codec::io::action_source_of(store::io::git_common_dir(checkout)),
        only);
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
    if (run.same_record)
        return false;
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

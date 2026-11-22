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
#include "compass.cli/app/sync.hpp"

#include <algorithm>
#include <chrono>
#include <deque>
#include <iterator>
#include <stdexcept>
#include "compass.cli/app/steps.hpp"
#include "compass.cli/config/repository_file.hpp"
#include "compass.cli/io/listing.hpp"
#include "compass.codec/ops/parallel_for.hpp"
#include "compass.db/ops/extract.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::cli::app {

namespace {

namespace fs = std::filesystem;
using store::types::blob_id;

/**
 * @brief How many times a sync loads what a collection removed under it
 * before it gives up.
 */
constexpr int write_attempts = 3;

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

template <typename Step>
void timed(sync_report& r, std::string_view name, Step&& step) {
    const auto start = std::chrono::steady_clock::now();
    step();
    r.steps.emplace_back(name, since(start));
}

/**
 * @brief The rows of @p files whose IDs are in @p wanted, one per ID, as
 * files of their own, loaded: each document's tables from the store, or
 * parsed. Objects parsed are written to the store, as a run writes them.
 */
corpus_files load_rows(const corpus_files& files,
    const std::vector<blob_id>& wanted, unsigned threads) {
    std::vector<std::string_view> paths;
    std::vector<std::size_t> rows;
    std::vector<blob_id> taken;
    for (std::size_t row = 0; row < files.paths.size(); ++row) {
        const auto& id = files.resolved[row].id;
        if (!files.errors[row].empty() ||
            !std::ranges::binary_search(wanted, id) ||
            std::ranges::contains(taken, id))
            continue;
        taken.push_back(id);
        paths.push_back(files.paths[row]);
        rows.push_back(row);
    }
    auto sub = open_files(files.checkout, files.packs, std::move(paths),
        files.commit);
    for (std::size_t i = 0; i < rows.size(); ++i)
        sub.resolved[i] = files.resolved[rows[i]];
    load(sub, threads);
    store_objects(sub);
    return sub;
}

std::vector<db::types::corpus_document> extract_all(
    const corpus_files& f, unsigned threads) {
    std::vector<db::types::corpus_document> out(f.paths.size());
    std::vector<std::uint8_t> ok(f.paths.size(), 0);
    codec::ops::parallel_for(f.paths.size(), threads, [&](std::size_t row) {
        if (!f.errors[row].empty())
            return;
        out[row] = db::ops::extract(f.resolved[row].id, f.texts[row],
            f.tables[row]);
        ok[row] = 1;
    });
    std::vector<db::types::corpus_document> kept;
    kept.reserve(out.size());
    for (std::size_t row = 0; row < out.size(); ++row)
        if (ok[row] != 0)
            kept.push_back(std::move(out[row]));
    return kept;
}

std::optional<std::string> head_of(const fs::path& checkout) {
    try {
        return store::ops::to_hex(io::resolve_commit(checkout, "HEAD"));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}

bool is_synced_document(const fs::path&, std::string_view path) {
    return path.ends_with(".org") && path != config::repository_file_name;
}

sync_report sync_environment(db::io::connection& c, const sync_request& r) {
    const auto start = std::chrono::steady_clock::now();
    sync_report out;
    out.environment = r.environment;
    const auto common = store::io::git_common_dir(r.checkout);
    std::optional<commit_source> commit;
    listing listed;
    timed(out, "list", [&] {
        if (r.commit) {
            commit.emplace(common, *r.commit);
            list(listed, *commit);
        } else {
            list(listed, r.checkout);
        }
    });
    auto files = open_files(r.checkout, store::io::packs_dir(common),
        listed.documents, commit ? &*commit : nullptr);
    timed(out, "resolve", [&] { resolve(files, r.threads); });

    std::vector<db::types::placement> placements;
    std::vector<blob_id> ids;
    for (std::size_t row = 0; row < files.paths.size(); ++row) {
        if (!files.errors[row].empty()) {
            out.errors.push_back(std::string(files.paths[row]) + ": " +
                files.errors[row]);
            continue;
        }
        placements.push_back(
            {std::string(files.paths[row]), files.resolved[row].id});
        ids.push_back(files.resolved[row].id);
    }
    std::ranges::sort(ids);
    ids.erase(std::ranges::unique(ids).begin(), ids.end());
    out.placed = placements.size();

    std::vector<blob_id> missing;
    timed(out, "present", [&] {
        auto have = db::io::present(c, ids);
        std::ranges::sort(have);
        std::ranges::set_difference(ids, have, std::back_inserter(missing));
    });

    db::types::environment e;
    e.name = r.environment;
    if (r.commit) {
        e.kind = db::types::environment_kind::release;
        e.head = store::ops::to_hex(*r.commit);
    } else {
        e.root = r.checkout.string();
        e.head = head_of(r.checkout);
    }

    std::deque<corpus_files> loads;
    std::vector<db::types::corpus_document> documents;
    for (int attempt = 0; attempt < write_attempts; ++attempt) {
        timed(out, "load", [&] {
            loads.push_back(load_rows(files, missing, r.threads));
        });
        const auto& loaded = loads.back();
        for (std::size_t row = 0; row < loaded.paths.size(); ++row) {
            if (loaded.errors[row].empty())
                ++(loaded.loaded[row] != 0 ? out.loaded : out.parsed);
        }
        timed(out, "extract", [&] {
            std::ranges::move(extract_all(loaded, r.threads),
                std::back_inserter(documents));
        });
        timed(out, "write", [&] {
            out.written = db::io::write_environment(c, e, documents,
                placements);
        });
        if (!out.written.retry)
            break;
        missing = out.written.missing;
        std::ranges::sort(missing);
    }
    if (out.written.retry)
        throw std::runtime_error("documents were collected while the sync "
            "wrote them, three times; run compass sync again");
    timed(out, "collect", [&] {
        out.collection = db::io::collect_when_grown(c);
    });
    out.ms = since(start);
    return out;
}

sync_report sync_file(db::io::connection& c, const fs::path& checkout,
    const std::string& environment, std::string_view path) {
    const auto start = std::chrono::steady_clock::now();
    sync_report out;
    out.environment = environment;
    db::types::environment e;
    e.name = environment;
    e.root = checkout.string();
    const std::vector<std::string> scope{std::string(path)};

    std::error_code ec;
    if (!fs::is_regular_file(checkout / path, ec)) {
        out.written = db::io::write_environment(c, e, {}, {}, scope);
        out.ms = since(start);
        return out;
    }
    const auto common = store::io::git_common_dir(checkout);
    auto files = open_files(checkout, store::io::packs_dir(common),
        {path});
    files.resolved[0].id = store::ops::blob_id_of(
        platform::filesystem::mapped_file(checkout / path).text());
    const std::vector<db::types::placement> placements{
        {std::string(path), files.resolved[0].id}};
    out.placed = 1;
    std::optional<corpus_files> loaded;
    std::vector<db::types::corpus_document> documents;
    for (int attempt = 0; attempt < write_attempts; ++attempt) {
        std::vector<blob_id> missing{files.resolved[0].id};
        if (attempt == 0 && !db::io::present(c, missing).empty())
            missing.clear();
        if (!missing.empty() && !loaded) {
            loaded.emplace(load_rows(files, missing, 1));
            if (!loaded->errors.empty() && !loaded->errors[0].empty())
                out.errors.push_back(std::string(path) + ": " +
                    loaded->errors[0]);
            else
                ++(loaded->loaded[0] != 0 ? out.loaded : out.parsed);
            documents = extract_all(*loaded, 1);
        }
        if (!out.errors.empty())
            break;
        out.written = db::io::write_environment(c, e, documents, placements,
            scope);
        if (!out.written.retry)
            break;
    }
    out.ms = since(start);
    return out;
}

}

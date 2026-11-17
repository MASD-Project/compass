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
#include "compass.cli/config/repository_file.hpp"
#include "compass.bench/ops/warm_stage.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "compass.cli/app/steps.hpp"
#include "compass.store/io/id_resolver.hpp"

namespace compass::bench::ops {

namespace {

using store::io::id_source;
namespace app = compass::cli::app;

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

types::warm_run run_once(const std::filesystem::path& checkout,
    const std::filesystem::path& objects,
    const std::vector<std::string_view>& paths, unsigned threads,
    types::warm_result& r) {
    types::warm_run run;
    auto files = app::open_files(checkout, objects, paths);

    auto start = std::chrono::steady_clock::now();
    app::resolve(files, threads);
    run.resolve_ms = since(start);
    r.git_index_problem = files.resolver->index_problem();
    r.worktree_index_problem = files.resolver->worktree_index_problem();
    for (std::size_t row = 0; row < paths.size(); ++row) {
        if (!files.errors[row].empty())
            continue;
        switch (files.resolved[row].source) {
        case id_source::git_index: ++run.from_git_index; break;
        case id_source::worktree_index: ++run.from_worktree_index; break;
        case id_source::hashed: ++run.hashed; break;
        }
    }

    start = std::chrono::steady_clock::now();
    app::load(files, threads);
    run.load_ms = since(start);

    start = std::chrono::steady_clock::now();
    run.written = app::store_objects(files);
    run.write_ms = since(start);

    start = std::chrono::steady_clock::now();
    r.index_entries = app::write_worktree_index(files);
    run.index_ms = since(start);

    for (std::size_t row = 0; row < paths.size(); ++row) {
        if (files.errors[row].empty()) {
            if (files.loaded[row] != 0)
                ++run.loaded;
            continue;
        }
        ++run.errors;
        if (r.first_error.empty())
            r.first_error =
                std::string(paths[row]) + ": " + files.errors[row];
    }
    return run;
}

}

types::warm_result run_warm_stage(const std::filesystem::path& checkout,
    const types::file_table& table, const std::filesystem::path& objects,
    unsigned threads, unsigned repetitions) {
    std::vector<std::string_view> paths;
    for (std::uint32_t id = 0; id < table.size(); ++id) {
        if (table.extensions[table.extension_ids[id]] == ".org" &&
            table.paths[id] != cli::config::repository_file_name)
            paths.emplace_back(table.paths[id]);
    }

    types::warm_result r;
    r.files = paths.size();
    for (unsigned rep = 0; rep < repetitions; ++rep)
        r.runs.push_back(run_once(checkout, objects, paths, threads, r));
    return r;
}

}

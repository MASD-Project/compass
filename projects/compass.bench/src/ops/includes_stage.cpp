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
#include "compass.bench/ops/includes_stage.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
#include "compass.bench/ops/include_graph.hpp"
#include "compass.bench/ops/include_resolve.hpp"
#include "compass.bench/ops/include_scan.hpp"
#include "compass.bench/ops/partition.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"

namespace compass::bench::ops {

namespace {

struct scan_tally {
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t errors = 0;
    std::string first_error;
    types::include_table includes;
};

scan_tally scan_range(const std::filesystem::path& checkout,
    const types::file_table& table, std::span<const std::uint32_t> ids) {
    scan_tally t;
    for (const auto id : ids) {
        try {
            const platform::filesystem::mapped_file file(
                checkout / table.paths[id]);
            scan_includes(file.text(), id, t.includes);
            t.bytes += file.size();
            ++t.files;
        } catch (const std::system_error& e) {
            if (t.errors == 0)
                t.first_error = e.what();
            ++t.errors;
        }
    }
    return t;
}

void append(types::include_table& to, const types::include_table& from) {
    const auto base = static_cast<std::uint32_t>(to.names.size());
    to.includers.insert(to.includers.end(), from.includers.begin(),
        from.includers.end());
    to.lines.insert(to.lines.end(), from.lines.begin(), from.lines.end());
    to.kinds.insert(to.kinds.end(), from.kinds.begin(), from.kinds.end());
    for (const auto offset : from.name_offsets)
        to.name_offsets.push_back(base + offset);
    to.name_sizes.insert(to.name_sizes.end(), from.name_sizes.begin(),
        from.name_sizes.end());
    to.names.append(from.names);
    to.computed += from.computed;
}

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

}

types::includes_result run_includes_stage(
    const std::filesystem::path& checkout, const types::file_table& table,
    const types::search_paths& paths, unsigned threads,
    unsigned repetitions) {
    std::vector<std::uint32_t> scanned;
    std::vector<std::uint64_t> scanned_sizes;
    types::includes_result r;
    for (std::uint32_t id = 0; id < table.size(); ++id) {
        const auto& extension = table.extensions[table.extension_ids[id]];
        if (!is_scanned_extension(extension))
            continue;
        scanned.push_back(id);
        scanned_sizes.push_back(table.sizes[id]);
        if (extension == ".cpp" || extension == ".cc" || extension == ".cxx")
            r.roots.push_back(id);
    }
    const auto ranges = balance_ranges(scanned_sizes, threads);

    std::vector<scan_tally> tallies(ranges.size());
    for (unsigned rep = 0; rep < repetitions; ++rep) {
        auto start = std::chrono::steady_clock::now();
        {
            std::vector<std::jthread> workers;
            workers.reserve(ranges.size());
            for (std::size_t i = 0; i < ranges.size(); ++i) {
                workers.emplace_back([&, i] {
                    tallies[i] = scan_range(checkout, table,
                        std::span(scanned).subspan(ranges[i].begin,
                            ranges[i].end - ranges[i].begin));
                });
            }
        }
        types::include_table includes;
        for (const auto& t : tallies)
            append(includes, t.includes);
        r.scan_ms.push_back(since(start));

        start = std::chrono::steady_clock::now();
        resolve_includes(includes, table, paths);
        r.resolve_ms.push_back(since(start));

        start = std::chrono::steady_clock::now();
        auto graph = build_include_graph(includes, table.size());
        r.graph_ms.push_back(since(start));

        start = std::chrono::steady_clock::now();
        r.transitive_fan_in = transitive_fan_in(graph, r.roots);
        r.closure_ms.push_back(since(start));

        r.includes = std::move(includes);
        r.graph = std::move(graph);
    }

    for (const auto& t : tallies) {
        r.files += t.files;
        r.bytes += t.bytes;
        r.errors += t.errors;
        if (r.first_error.empty())
            r.first_error = t.first_error;
    }
    r.direct_fan_in = direct_fan_in(r.graph);
    return r;
}

}

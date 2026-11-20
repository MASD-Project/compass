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
#include "compass.bench/ops/org_stage.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
#include "compass.bench/ops/partition.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"

namespace compass::bench::ops {

namespace {

struct tally {
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t errors = 0;
    std::uint64_t elements = 0;
    std::uint64_t objects = 0;
    std::uint64_t headings = 0;
    std::uint64_t links = 0;
    std::uint64_t names = 0;
    std::string first_error;
};

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

tally parse_range(const std::filesystem::path& checkout,
    const types::file_table& table, types::org_result& r,
    types::file_range range, bool first) {
    tally t;
    compass::org::types::tables tables;
    for (std::size_t row = range.begin; row < range.end; ++row) {
        try {
            const platform::filesystem::mapped_file file(
                checkout / table.paths[r.ids[row]]);
            const auto start = std::chrono::steady_clock::now();
            ingest::org::parse(file.text(), tables);
            const double ms = since(start);
            r.parse_ms[row] = first ? ms : std::min(r.parse_ms[row], ms);
            t.elements += tables.elements.size();
            t.objects += tables.objects.size();
            t.headings += tables.headings.size();
            t.links += tables.links.size();
            t.names += tables.names.size();
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

}

types::org_result run_org_stage(const std::filesystem::path& checkout,
    const types::file_table& table, unsigned threads, unsigned repetitions) {
    types::org_result r;
    r.stage.name = "org";
    r.stage.wall_ms.reserve(repetitions);
    std::vector<std::uint64_t> sizes;
    for (std::uint32_t id = 0; id < table.size(); ++id) {
        if (table.extensions[table.extension_ids[id]] == ".org") {
            r.ids.push_back(id);
            sizes.push_back(table.sizes[id]);
        }
    }
    r.parse_ms.assign(r.ids.size(), 0);
    const auto ranges = balance_ranges(sizes, threads);

    std::vector<tally> tallies(ranges.size());
    for (unsigned rep = 0; rep < repetitions; ++rep) {
        const auto start = std::chrono::steady_clock::now();
        {
            std::vector<std::jthread> workers;
            workers.reserve(ranges.size());
            for (std::size_t i = 0; i < ranges.size(); ++i) {
                workers.emplace_back([&, i] {
                    tallies[i] = parse_range(checkout, table, r, ranges[i],
                        rep == 0);
                });
            }
        }
        r.stage.wall_ms.push_back(since(start));
    }

    for (const auto& t : tallies) {
        r.stage.files += t.files;
        r.stage.bytes += t.bytes;
        r.stage.errors += t.errors;
        r.elements += t.elements;
        r.objects += t.objects;
        r.headings += t.headings;
        r.links += t.links;
        r.names += t.names;
        if (r.stage.first_error.empty())
            r.stage.first_error = t.first_error;
    }
    return r;
}

}

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
#include "compass.bench/ops/read_stage.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
#include "compass.platform/filesystem/mapped_file.hpp"

namespace compass::bench::ops {

namespace {

struct tally {
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t errors = 0;
    std::uint64_t checksum = 0;
    std::string first_error;
};

tally read_range(const std::filesystem::path& checkout,
    const types::file_table& table, types::file_range range) {
    tally t;
    for (auto id = range.begin; id < range.end; ++id) {
        try {
            const platform::filesystem::mapped_file file(
                checkout / table.paths[id]);
            const auto* p =
                reinterpret_cast<const unsigned char*>(file.bytes().data());
            std::uint64_t sum = 0;
            for (std::size_t i = 0, n = file.size(); i < n; ++i)
                sum += p[i];
            t.checksum += sum;
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

types::stage_result run_read_stage(const std::filesystem::path& checkout,
    const types::file_table& table, std::span<const types::file_range> ranges,
    unsigned repetitions) {
    types::stage_result r;
    r.name = "read";
    r.wall_ms.reserve(repetitions);

    std::vector<tally> tallies(ranges.size());
    for (unsigned rep = 0; rep < repetitions; ++rep) {
        const auto start = std::chrono::steady_clock::now();
        {
            std::vector<std::jthread> threads;
            threads.reserve(ranges.size());
            for (std::size_t i = 0; i < ranges.size(); ++i) {
                threads.emplace_back([&, i] {
                    tallies[i] = read_range(checkout, table, ranges[i]);
                });
            }
        }
        const auto stop = std::chrono::steady_clock::now();
        r.wall_ms.push_back(
            std::chrono::duration<double, std::milli>(stop - start).count());
    }

    for (const auto& t : tallies) {
        r.files += t.files;
        r.bytes += t.bytes;
        r.errors += t.errors;
        r.checksum += t.checksum;
        if (r.first_error.empty())
            r.first_error = t.first_error;
    }
    return r;
}

}

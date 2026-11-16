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
#include "compass.bench/ops/ids_stage.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include "compass.bench/ops/listing.hpp"
#include "compass.bench/ops/partition.hpp"
#include "compass.store/io/git_files.hpp"
#include "compass.store/io/id_resolver.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::bench::ops {

namespace {

using store::io::id_source;

struct tally {
    std::uint64_t files = 0;
    std::uint64_t worktree = 0;
    std::uint64_t bytes = 0;
    std::uint64_t errors = 0;
    std::string first_error;
};

/**
 * @brief The outcome of one repetition, one row per file of the table.
 * resolved is 0 for a file that could not be hashed.
 */
struct resolution {
    std::vector<store::types::blob_id> ids;
    std::vector<id_source> sources;
    std::vector<std::uint8_t> resolved;
};

std::string_view source_name(id_source s) {
    switch (s) {
    case id_source::git_index: return "git index";
    case id_source::worktree_index: return "worktree index";
    case id_source::hashed: return "hashed";
    }
    return "unknown";
}

/**
 * @brief Up to @p count file ids spread evenly over the table, leaving out
 * the paths git hash-object --stdin-paths would read differently.
 */
std::vector<std::uint32_t> sample(const types::file_table& table,
    const resolution& r, std::uint64_t count) {
    std::vector<std::uint32_t> picked;
    const auto n = table.size();
    const auto take = std::min<std::uint64_t>(count, n);
    for (std::uint64_t k = 0; k < take; ++k) {
        const auto id = static_cast<std::uint32_t>(k * n / take);
        const auto& path = table.paths[id];
        if (r.resolved[id] != 0 && !path.starts_with('"') &&
            path.find('\n') == std::string::npos)
            picked.push_back(id);
    }
    return picked;
}

void verify_ids(const std::filesystem::path& checkout,
    const types::file_table& table, const resolution& r, std::uint64_t count,
    types::ids_result& result) {
    const auto picked = sample(table, r, count);
    if (picked.empty())
        return;
    std::vector<std::string_view> paths;
    paths.reserve(picked.size());
    for (const auto id : picked)
        paths.emplace_back(table.paths[id]);
    const auto expected = git_hash_objects(checkout, paths);
    for (std::size_t i = 0; i < picked.size(); ++i) {
        const auto id = picked[i];
        const auto actual = store::ops::to_hex(r.ids[id]);
        ++result.verified;
        if (actual == expected[i])
            continue;
        if (result.mismatches++ == 0)
            result.first_mismatch = table.paths[id] + " (" +
                std::string(source_name(r.sources[id])) +
                "): compass " + actual + ", git " + expected[i];
    }
}

}

types::ids_result run_ids_stage(const std::filesystem::path& checkout,
    const types::file_table& table, unsigned threads, unsigned repetitions,
    std::uint64_t verify) {
    types::ids_result result;
    result.files = table.size();
    const std::vector<std::uint64_t> ones(table.size(), 1);
    const auto stat_ranges = balance_ranges(ones, threads);
    resolution r;

    for (unsigned rep = 0; rep < repetitions; ++rep) {
        r = {std::vector<store::types::blob_id>(table.size()),
            std::vector<id_source>(table.size(), id_source::hashed),
            std::vector<std::uint8_t>(table.size(), 0)};

        const auto start = std::chrono::steady_clock::now();
        const store::io::id_resolver resolver(checkout);
        const std::chrono::duration<double, std::milli> read =
            std::chrono::steady_clock::now() - start;
        result.read_ms.push_back(read.count());
        result.index_version = resolver.index().version;
        result.index_entries = resolver.index().size();
        result.trusted_entries = resolver.trusted_entries();
        result.autocrlf = resolver.config().autocrlf;
        result.index_problem = resolver.index_problem();

        const auto [stat_ms, stat_tallies] = run_ranges(stat_ranges,
            [&](types::file_range range) {
                tally t;
                for (auto id = range.begin; id < range.end; ++id) {
                    if (const auto found = resolver.known_id(table.paths[id])) {
                        r.ids[id] = found->id;
                        r.sources[id] = found->source;
                        r.resolved[id] = 1;
                        ++(found->source == id_source::git_index ? t.files
                                                                 : t.worktree);
                    }
                }
                return t;
            });
        result.stat_ms.push_back(stat_ms);

        std::vector<std::uint32_t> pending;
        std::vector<std::uint64_t> pending_sizes;
        for (std::uint32_t id = 0; id < table.size(); ++id) {
            if (r.resolved[id] == 0) {
                pending.push_back(id);
                pending_sizes.push_back(table.sizes[id]);
            }
        }
        const auto [hash_ms, hash_tallies] = run_ranges(
            balance_ranges(pending_sizes, threads),
            [&](types::file_range range) {
                tally t;
                for (auto row = range.begin; row < range.end; ++row) {
                    const auto id = pending[row];
                    try {
                        r.ids[id] = store::io::hash_file(
                            checkout / table.paths[id]);
                        r.resolved[id] = 1;
                        t.bytes += table.sizes[id];
                        ++t.files;
                    } catch (const std::system_error& e) {
                        if (t.errors++ == 0)
                            t.first_error = table.paths[id] + ": " + e.what();
                    }
                }
                return t;
            });
        result.hash_ms.push_back(hash_ms);

        result.from_index = result.from_worktree = 0;
        result.hashed = result.hashed_bytes = 0;
        result.errors = 0;
        result.first_error.clear();
        for (const auto& t : stat_tallies) {
            result.from_index += t.files;
            result.from_worktree += t.worktree;
        }
        for (const auto& t : hash_tallies) {
            result.hashed += t.files;
            result.hashed_bytes += t.bytes;
            result.errors += t.errors;
            if (result.first_error.empty())
                result.first_error = t.first_error;
        }
    }

    verify_ids(checkout, table, r, verify, result);
    return result;
}

}

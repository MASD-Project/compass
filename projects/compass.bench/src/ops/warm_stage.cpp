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
#include "compass.bench/ops/warm_stage.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include "compass.bench/ops/partition.hpp"
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
using store::io::id_source;
namespace ot = compass::org::types;

/**
 * @brief The org files of the checkout, one row each, and what the current
 * repetition knows of them. ok is 0 for a row that failed, and missing is 1
 * for a row whose object must be written.
 */
struct corpus {
    const std::filesystem::path& checkout;
    const std::filesystem::path& objects;
    std::vector<std::string_view> paths;
    std::vector<std::uint64_t> sizes;
    std::vector<store::io::resolved_id> resolved;
    std::vector<std::uint8_t> ok;
    std::vector<std::uint8_t> missing;

    [[nodiscard]] std::filesystem::path object(std::size_t row) const {
        return store::io::object_path(objects, resolved[row].id);
    }

    [[nodiscard]] compass::org::io::object_key key(std::size_t row) const {
        return {resolved[row].id.bytes, ingest::org::parser_version};
    }
};

struct tally {
    std::uint64_t done = 0;
    std::uint64_t errors = 0;
    std::uint64_t by_source[3] = {};
    std::string first_error;

    void error(std::string_view path, const std::system_error& e) {
        if (errors++ == 0)
            first_error = std::string(path) + ": " + e.what();
    }
};

tally resolve_range(corpus& c, const store::io::id_resolver& resolver,
    types::file_range range) {
    tally t;
    for (auto row = range.begin; row < range.end; ++row) {
        try {
            c.resolved[row] = resolver.resolve(c.paths[row]);
            c.ok[row] = 1;
            ++t.by_source[static_cast<std::size_t>(c.resolved[row].source)];
        } catch (const std::system_error& e) {
            c.resolved[row] = {};
            c.ok[row] = 0;
            t.error(c.paths[row], e);
        }
    }
    return t;
}

tally load_range(corpus& c, types::file_range range) {
    tally t;
    for (auto row = range.begin; row < range.end; ++row) {
        c.missing[row] = c.ok[row];
        if (c.ok[row] == 0)
            continue;
        try {
            const mapped_file file(c.object(row));
            if (compass::org::io::decode(file.bytes(), c.key(row)) &&
                compass::org::io::checksum_matches(file.bytes())) {
                c.missing[row] = 0;
                ++t.done;
            }
        } catch (const std::system_error&) {
        }
    }
    return t;
}

tally write_range(const corpus& c, const std::vector<std::uint32_t>& rows,
    types::file_range range) {
    tally t;
    ot::tables tables;
    std::vector<std::byte> bytes;
    for (auto at = range.begin; at < range.end; ++at) {
        const auto row = rows[at];
        try {
            const mapped_file source(c.checkout / c.paths[row]);
            ingest::org::parse(source.text(), tables);
            compass::org::io::encode(ot::view(tables), c.key(row), bytes);
            store::io::write_object(c.object(row), bytes);
            ++t.done;
        } catch (const std::system_error& e) {
            t.error(c.paths[row], e);
        }
    }
    return t;
}

void add_errors(const std::vector<tally>& tallies, types::warm_run& run,
    types::warm_result& r) {
    for (const auto& t : tallies) {
        run.errors += t.errors;
        if (r.first_error.empty())
            r.first_error = t.first_error;
    }
}

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

types::warm_run run_once(corpus& c,
    const std::vector<types::file_range>& ranges, unsigned threads,
    types::warm_result& r) {
    types::warm_run run;

    const auto start = std::chrono::steady_clock::now();
    const store::io::id_resolver resolver(c.checkout);
    const auto resolved = run_ranges(ranges,
        [&](types::file_range range) {
            return resolve_range(c, resolver, range);
        }).second;
    run.resolve_ms = since(start);
    for (const auto& t : resolved) {
        run.from_git_index += t.by_source[
            static_cast<std::size_t>(id_source::git_index)];
        run.from_worktree_index += t.by_source[
            static_cast<std::size_t>(id_source::worktree_index)];
        run.hashed += t.by_source[static_cast<std::size_t>(id_source::hashed)];
    }
    add_errors(resolved, run, r);
    r.git_index_problem = resolver.index_problem();
    r.worktree_index_problem = resolver.worktree_index_problem();

    const auto [load_ms, loaded] = run_ranges(ranges,
        [&](types::file_range range) { return load_range(c, range); });
    run.load_ms = load_ms;
    for (const auto& t : loaded)
        run.loaded += t.done;

    std::vector<std::uint32_t> rows;
    std::vector<std::uint64_t> sizes;
    for (std::uint32_t row = 0; row < c.paths.size(); ++row) {
        if (c.missing[row] != 0) {
            rows.push_back(row);
            sizes.push_back(c.sizes[row]);
        }
    }
    const auto [write_ms, written] = run_ranges(
        balance_ranges(sizes, threads),
        [&](types::file_range range) { return write_range(c, rows, range); });
    run.write_ms = write_ms;
    for (const auto& t : written)
        run.written += t.done;
    add_errors(written, run, r);

    const auto index_start = std::chrono::steady_clock::now();
    r.index_entries = resolver.write_worktree_index(c.paths, c.resolved);
    run.index_ms = since(index_start);
    return run;
}

}

types::warm_result run_warm_stage(const std::filesystem::path& checkout,
    const types::file_table& table, const std::filesystem::path& objects,
    unsigned threads, unsigned repetitions) {
    corpus c{checkout, objects, {}, {}, {}, {}, {}};
    for (std::uint32_t id = 0; id < table.size(); ++id) {
        if (table.extensions[table.extension_ids[id]] == ".org") {
            c.paths.emplace_back(table.paths[id]);
            c.sizes.push_back(table.sizes[id]);
        }
    }
    c.resolved.resize(c.paths.size());
    c.ok.assign(c.paths.size(), 0);
    c.missing.assign(c.paths.size(), 0);
    const auto ranges = balance_ranges(c.sizes, threads);

    types::warm_result r;
    r.files = c.paths.size();
    for (unsigned rep = 0; rep < repetitions; ++rep)
        r.runs.push_back(run_once(c, ranges, threads, r));
    return r;
}

}

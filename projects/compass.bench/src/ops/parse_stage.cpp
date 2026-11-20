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
#include "compass.bench/ops/parse_stage.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
#include <tree_sitter/api.h>
#include "compass.bench/ops/parse.hpp"
#include "compass.bench/ops/partition.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"

namespace compass::bench::ops {

namespace {

struct parser_deleter {
    void operator()(TSParser* p) const noexcept { ts_parser_delete(p); }
};

struct tree_deleter {
    void operator()(TSTree* t) const noexcept { ts_tree_delete(t); }
};

using parser_ptr = std::unique_ptr<TSParser, parser_deleter>;
using tree_ptr = std::unique_ptr<TSTree, tree_deleter>;

struct tally {
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t errors = 0;
    std::string first_error;
};

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

parser_ptr make_parser(types::language l) {
    parser_ptr parser(ts_parser_new());
    if (!ts_parser_set_language(parser.get(), grammar_of(l)))
        throw std::runtime_error("tree-sitter rejected the " +
            std::string(name_of(l)) + " grammar: ABI version mismatch");
    return parser;
}

void record_error(tally& t, const std::string& what) {
    if (t.errors == 0)
        t.first_error = what;
    ++t.errors;
}

tally parse_range(const std::filesystem::path& checkout,
    const types::file_table& table, types::language l,
    types::language_result& r, types::file_range range, bool first) {
    tally t;
    const auto parser = make_parser(l);
    for (std::size_t row = range.begin; row < range.end; ++row) {
        const auto& path = table.paths[r.ids[row]];
        try {
            const platform::filesystem::mapped_file file(checkout / path);
            const auto text = file.text();
            const auto start = std::chrono::steady_clock::now();
            const tree_ptr tree(ts_parser_parse_string(parser.get(), nullptr,
                text.data(), static_cast<std::uint32_t>(text.size())));
            const double ms = since(start);
            if (!tree) {
                record_error(t, path + ": parser returned no tree");
                continue;
            }
            r.error_nodes[row] =
                count_error_nodes(ts_tree_root_node(tree.get()));
            r.parse_ms[row] = first ? ms : std::min(r.parse_ms[row], ms);
            t.bytes += file.size();
            ++t.files;
        } catch (const std::system_error& e) {
            record_error(t, e.what());
        }
    }
    return t;
}

void run_language(const std::filesystem::path& checkout,
    const types::file_table& table, types::language l,
    types::language_result& r, unsigned threads, unsigned repetitions) {
    std::vector<std::uint64_t> sizes;
    for (const auto id : r.ids)
        sizes.push_back(table.sizes[id]);
    r.parse_ms.assign(r.ids.size(), 0);
    r.error_nodes.assign(r.ids.size(), 0);
    r.stage.name = name_of(l);
    r.stage.wall_ms.reserve(repetitions);
    const auto ranges = balance_ranges(sizes, threads);

    std::vector<tally> tallies(ranges.size());
    for (unsigned rep = 0; rep < repetitions; ++rep) {
        const auto start = std::chrono::steady_clock::now();
        {
            std::vector<std::jthread> workers;
            workers.reserve(ranges.size());
            for (std::size_t i = 0; i < ranges.size(); ++i) {
                workers.emplace_back([&, i] {
                    tallies[i] = parse_range(checkout, table, l, r, ranges[i],
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
        if (r.stage.first_error.empty())
            r.stage.first_error = t.first_error;
    }
}

}

types::parse_result run_parse_stage(const std::filesystem::path& checkout,
    const types::file_table& table, unsigned threads, unsigned repetitions) {
    types::parse_result result;
    for (std::uint32_t id = 0; id < table.size(); ++id) {
        const auto l = language_of(table.extensions[table.extension_ids[id]]);
        if (l)
            result.languages[static_cast<std::size_t>(*l)].ids.push_back(id);
    }
    for (const auto l : {types::language::cpp, types::language::sql}) {
        run_language(checkout, table, l,
            result.languages[static_cast<std::size_t>(l)], threads,
            repetitions);
    }
    return result;
}

}

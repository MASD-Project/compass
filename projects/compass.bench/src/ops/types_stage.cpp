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
#include "compass.bench/ops/types_stage.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>
#include "compass.bench/ops/partition.hpp"
#include "compass.ingest/meta/type_registry_builder.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/ops/type_lookup.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"

namespace compass::bench::ops {

namespace {

namespace ot = compass::org::types;

struct tally {
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t errors = 0;
    double resolve_ms = 0;
    std::string first_error;
};

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' ||
        s.back() == '\r' || s.back() == '\n'))
        s.remove_suffix(1);
    return s;
}

/**
 * @brief The value of the first #+type: keyword of a parsed document.
 */
std::optional<std::string_view> type_keyword(std::string_view source,
    const ot::tables_view& t) noexcept {
    for (std::uint32_t k = 0; k < t.keywords.size(); ++k) {
        const ot::keyword_id id{k};
        const auto n = t.keywords.names[id];
        const auto first = t.names.firsts[n];
        const auto end = n.next().value < t.names.size()
            ? t.names.firsts[n.next()]
            : static_cast<std::uint32_t>(t.name_bytes.size());
        if (std::string_view(t.name_bytes.data() + first, end - first) ==
            "type")
            return trim(t.keywords.values[id].of(source));
    }
    return std::nullopt;
}

tally resolve_range(const std::filesystem::path& checkout,
    const types::file_table& table, types::types_result& r,
    types::file_range range) {
    tally t;
    ot::tables tables;
    for (auto row = range.begin; row < range.end; ++row) {
        try {
            const platform::filesystem::mapped_file file(
                checkout / table.paths[r.ids[row]]);
            ingest::org::parse(file.text(), tables);
            const auto start = std::chrono::steady_clock::now();
            const auto value = type_keyword(file.text(), ot::view(tables));
            const auto type = value ? org::ops::find_type(r.meta.registry,
                *value) : ot::type_id::none();
            t.resolve_ms += since(start);
            r.types[row] = type;
            r.outcomes[row] = !value ? types::resolution::untyped
                : type.is_none() ? types::resolution::unresolved
                                 : types::resolution::resolved;
            if (r.outcomes[row] == types::resolution::unresolved)
                r.values[row] = *value;
            else
                r.values[row].clear();
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

types::meta_registry build_meta_registry(const std::filesystem::path& meta) {
    std::vector<std::filesystem::path> pages;
    for (const auto& entry : std::filesystem::directory_iterator(meta)) {
        const auto name = entry.path().filename().string();
        if (entry.is_regular_file() && name.starts_with("document_type_") &&
            name.ends_with(".org"))
            pages.push_back(entry.path());
    }
    std::ranges::sort(pages);

    types::meta_registry r;
    ingest::meta::type_registry_builder builder;
    ot::tables tables;
    for (const auto& page : pages) {
        const platform::filesystem::mapped_file file(page);
        ingest::org::parse(file.text(), tables);
        r.paths.push_back(page.filename().string());
        r.reports.push_back(builder.add_contract(file.text(), ot::view(tables)));
    }
    for (const auto name : ingest::meta::model_types)
        builder.add_model(name);
    r.registry = builder.build();
    return r;
}

types::types_result run_types_stage(const std::filesystem::path& checkout,
    const types::file_table& table, const std::filesystem::path& meta,
    unsigned threads, unsigned repetitions) {
    types::types_result r;
    r.stage.name = "types";
    r.stage.wall_ms.reserve(repetitions);
    std::vector<std::uint64_t> sizes;
    for (std::uint32_t id = 0; id < table.size(); ++id) {
        if (table.extensions[table.extension_ids[id]] == ".org") {
            r.ids.push_back(id);
            sizes.push_back(table.sizes[id]);
        }
    }
    r.outcomes.assign(r.ids.size(), types::resolution::untyped);
    r.types.assign(r.ids.size(), ot::type_id::none());
    r.values.assign(r.ids.size(), {});
    const auto ranges = balance_ranges(sizes, threads);

    std::vector<tally> tallies(ranges.size());
    for (unsigned rep = 0; rep < repetitions; ++rep) {
        auto start = std::chrono::steady_clock::now();
        r.meta = build_meta_registry(meta);
        r.registry_ms.push_back(since(start));

        start = std::chrono::steady_clock::now();
        {
            std::vector<std::jthread> workers;
            workers.reserve(ranges.size());
            for (std::size_t i = 0; i < ranges.size(); ++i) {
                workers.emplace_back([&, i] {
                    tallies[i] = resolve_range(checkout, table, r, ranges[i]);
                });
            }
        }
        r.stage.wall_ms.push_back(since(start));
    }

    for (const auto& t : tallies) {
        r.stage.files += t.files;
        r.stage.bytes += t.bytes;
        r.stage.errors += t.errors;
        r.resolve_ms += t.resolve_ms;
        if (r.stage.first_error.empty())
            r.stage.first_error = t.first_error;
    }
    return r;
}

}

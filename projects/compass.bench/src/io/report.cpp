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
#include "compass.bench/io/report.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numeric>
#include <vector>

namespace compass::bench::io {

namespace {

constexpr std::size_t top_extensions = 15;

double median(std::vector<double> v) {
    std::ranges::sort(v);
    const auto n = v.size();
    return n % 2 == 1 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
}

void write_stage(std::ostream& os, const types::stage_result& stage) {
    os << std::format("{:<8}{:>10}{:>16}{:>8}{:>12}{:>10}{:>10}{:>10}\n",
        "stage", "files", "bytes", "errors", "median_ms", "min_ms", "max_ms",
        "MB/s");
    const auto [min, max] = std::ranges::minmax(stage.wall_ms);
    const double med = median(stage.wall_ms);
    const double mb_per_s = med > 0 ? (stage.bytes / 1e6) / (med / 1e3) : 0;
    os << std::format(
        "{:<8}{:>10}{:>16}{:>8}{:>12.2f}{:>10.2f}{:>10.2f}{:>10.1f}\n",
        stage.name, stage.files, stage.bytes, stage.errors, med, min, max,
        mb_per_s);
    os << std::format("repetitions: {}  checksum: {:#018x}\n",
        stage.wall_ms.size(), stage.checksum);
}

void write_extensions(std::ostream& os, const types::file_table& table) {
    const auto n = table.extensions.size();
    std::vector<std::uint64_t> files(n), bytes(n);
    for (std::size_t i = 0; i < table.size(); ++i) {
        ++files[table.extension_ids[i]];
        bytes[table.extension_ids[i]] += table.sizes[i];
    }

    std::vector<std::uint32_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::ranges::sort(order, [&](auto a, auto b) {
        return bytes[a] != bytes[b] ? bytes[a] > bytes[b] : files[a] > files[b];
    });
    order.resize(std::min(n, top_extensions));

    os << std::format("{:<16}{:>10}{:>16}\n", "extension", "files", "bytes");
    for (const auto id : order) {
        const auto& name = table.extensions[id];
        os << std::format("{:<16}{:>10}{:>16}\n",
            name.empty() ? "(none)" : name, files[id], bytes[id]);
    }
}

}

bool failed(const types::stage_result& stage) {
    return stage.files == 0 || stage.errors != 0;
}

void write_report(std::ostream& os, const types::stage_result& stage,
    const setup_timing& setup, const types::file_table& table) {
    write_stage(os, stage);
    os << std::format("threads: {}\n", setup.threads);
    os << std::format("git ls-files: {:.2f} ms\n", setup.listing_ms);
    os << std::format("file table (lstat): {:.2f} ms\n", setup.table_ms);
    os << "\n";
    write_extensions(os, table);

    if (stage.files == 0)
        os << std::format("\nFAILED: stage '{}' read zero files.\n",
            stage.name);
    if (stage.errors != 0)
        os << std::format("\nFAILED: stage '{}' hit {} error(s). First: {}\n",
            stage.name, stage.errors, stage.first_error);
}

}

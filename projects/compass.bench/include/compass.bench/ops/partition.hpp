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
#ifndef COMPASS_BENCH_OPS_PARTITION_HPP
#define COMPASS_BENCH_OPS_PARTITION_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include "compass.bench/types/file_table.hpp"

namespace compass::bench::ops {

/**
 * @brief Splits file ids into @p count contiguous ranges of similar bytes.
 *
 * Returns exactly max(count, 1) ranges that tile [0, sizes.size()) in order;
 * some may be empty. Each range's bytes differ from the mean by at most the
 * largest file.
 */
[[nodiscard]] std::vector<types::file_range> balance_ranges(
    std::span<const std::uint64_t> sizes, std::size_t count);

/**
 * @brief Runs @p work over each range on its own thread and returns the
 * wall time in milliseconds and what each call returned, in range order.
 */
template <typename Work>
auto run_ranges(const std::vector<types::file_range>& ranges, Work work) {
    std::vector<std::invoke_result_t<Work&, types::file_range>> results(
        ranges.size());
    const auto start = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> workers;
        workers.reserve(ranges.size());
        for (std::size_t i = 0; i < ranges.size(); ++i)
            workers.emplace_back([&, i] { results[i] = work(ranges[i]); });
    }
    const std::chrono::duration<double, std::milli> wall =
        std::chrono::steady_clock::now() - start;
    return std::pair{wall.count(), std::move(results)};
}

}

#endif

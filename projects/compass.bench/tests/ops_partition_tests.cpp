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
#include "compass.bench/ops/partition.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace {

using compass::bench::ops::balance_ranges;

void check_balanced(const std::vector<std::uint64_t>& sizes,
    std::size_t count) {
    INFO("files " << sizes.size() << ", count " << count);
    const auto ranges = balance_ranges(sizes, count);

    REQUIRE(ranges.size() == std::max<std::size_t>(count, 1));
    CHECK(ranges.front().begin == 0);
    CHECK(ranges.back().end == sizes.size());
    for (std::size_t i = 0; i < ranges.size(); ++i) {
        CHECK(ranges[i].begin <= ranges[i].end);
        if (i > 0)
            CHECK(ranges[i].begin == ranges[i - 1].end);
    }

    const auto total =
        std::accumulate(sizes.begin(), sizes.end(), std::uint64_t{0});
    const auto largest =
        sizes.empty() ? 0.0 : static_cast<double>(std::ranges::max(sizes));
    const double mean = static_cast<double>(total) / ranges.size();
    for (const auto& r : ranges) {
        const auto bytes = std::accumulate(sizes.begin() + r.begin,
            sizes.begin() + r.end, std::uint64_t{0});
        INFO("range [" << r.begin << ", " << r.end << ") bytes " << bytes
            << " mean " << mean << " largest " << largest);
        CHECK(static_cast<double>(bytes) <= mean + largest);
        CHECK(static_cast<double>(bytes) >= mean - largest);
    }
}

}

TEST_CASE("balance_ranges tiles random tables within one file of the mean",
    "[ops][partition]") {
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<std::uint64_t> small(0, 4096);
    for (const std::size_t files : {1u, 2u, 7u, 100u, 5000u}) {
        std::vector<std::uint64_t> sizes(files);
        for (auto& s : sizes)
            s = small(rng);
        for (const std::size_t count : {1u, 2u, 3u, 8u, 16u})
            check_balanced(sizes, count);
    }
}

TEST_CASE("balance_ranges handles one dominant file",
    "[ops][partition]") {
    std::vector<std::uint64_t> sizes(50, 10);
    sizes[25] = 1'000'000;
    check_balanced(sizes, 4);
}

TEST_CASE("balance_ranges with more threads than files leaves some empty",
    "[ops][partition]") {
    check_balanced({5, 5, 5}, 8);
}

TEST_CASE("balance_ranges of zero-byte or no files still tiles",
    "[ops][partition]") {
    check_balanced({}, 4);
    check_balanced({0, 0, 0, 0}, 3);
}

TEST_CASE("balance_ranges treats a zero count as one range",
    "[ops][partition]") {
    const std::vector<std::uint64_t> sizes{1, 2, 3};

    const auto ranges = balance_ranges(sizes, 0);

    REQUIRE(ranges.size() == 1);
    CHECK(ranges[0].begin == 0);
    CHECK(ranges[0].end == 3);
}

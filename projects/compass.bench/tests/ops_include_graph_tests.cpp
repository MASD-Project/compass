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
#include "compass.bench/ops/include_graph.hpp"

#include <cstdint>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::bench;

types::include_table resolved(
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& rows) {
    types::include_table t;
    for (const auto& [includer, target] : rows) {
        t.includers.push_back(includer);
        t.targets.push_back(target);
    }
    return t;
}

/**
 * @brief Two roots over a diamond with a cycle that leads back to a root.
 *
 * 0 and 5 are the roots. 0 includes 1 and 2, which both include 3; 1
 * names 3 twice. 3 and 4 include each other, and 4 includes root 0.
 * 5 includes 2. 0 also has an external and an unresolved include.
 */
types::include_table diamond() {
    return resolved({{0, 1}, {0, 2}, {0, types::external_target},
        {1, 3}, {1, 3}, {2, 3}, {3, 4}, {4, 3}, {4, 0},
        {0, types::unresolved_target}, {5, 2}});
}

}

TEST_CASE("build_include_graph keeps one sorted edge per pair",
    "[ops][include_graph]") {
    const auto g = ops::build_include_graph(diamond(), 6);

    CHECK(g.files() == 6);
    CHECK(g.offsets == std::vector<std::uint32_t>{0, 2, 3, 4, 5, 7, 8});
    CHECK(g.targets == std::vector<std::uint32_t>{1, 2, 3, 3, 4, 0, 3, 2});
}

TEST_CASE("build_include_graph gives every file a row, even with no edges",
    "[ops][include_graph]") {
    const auto g = ops::build_include_graph(resolved({{2, 0}}), 4);

    CHECK(g.offsets == std::vector<std::uint32_t>{0, 0, 0, 1, 1});
    CHECK(g.includes(2).size() == 1);
    CHECK(g.includes(3).empty());
}

TEST_CASE("direct_fan_in counts distinct includers", "[ops][include_graph]") {
    const auto g = ops::build_include_graph(diamond(), 6);

    CHECK(ops::direct_fan_in(g) ==
        std::vector<std::uint32_t>{1, 1, 2, 3, 1, 0});
}

TEST_CASE("transitive_fan_in counts each root once across diamonds and "
          "cycles",
    "[ops][include_graph]") {
    const auto g = ops::build_include_graph(diamond(), 6);
    const std::vector<std::uint32_t> roots = {0, 5};

    CHECK(ops::transitive_fan_in(g, roots) ==
        std::vector<std::uint32_t>{1, 2, 2, 2, 2, 0});
}

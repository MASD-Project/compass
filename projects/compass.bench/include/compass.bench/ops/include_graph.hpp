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
#ifndef COMPASS_BENCH_OPS_INCLUDE_GRAPH_HPP
#define COMPASS_BENCH_OPS_INCLUDE_GRAPH_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "compass.bench/types/include_graph.hpp"
#include "compass.bench/types/include_table.hpp"

namespace compass::bench::ops {

/**
 * @brief Builds the graph over @p files ids from resolved @p includes.
 *
 * Rows whose target is external or unresolved are dropped, and an includer
 * that names the same target twice gets one edge.
 */
[[nodiscard]] types::include_graph build_include_graph(
    const types::include_table& includes, std::size_t files);

/**
 * @brief How many distinct files include each file directly.
 */
[[nodiscard]] std::vector<std::uint32_t> direct_fan_in(
    const types::include_graph& graph);

/**
 * @brief How many of @p roots reach each file through one or more edges.
 *
 * A root does not count towards itself, even on a cycle back to it, and a
 * file reached along several paths from one root counts once.
 */
[[nodiscard]] std::vector<std::uint32_t> transitive_fan_in(
    const types::include_graph& graph, std::span<const std::uint32_t> roots);

}

#endif

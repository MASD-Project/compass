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
#ifndef COMPASS_BENCH_TYPES_INCLUDES_RESULT_HPP
#define COMPASS_BENCH_TYPES_INCLUDES_RESULT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "compass.bench/types/include_graph.hpp"
#include "compass.bench/types/include_table.hpp"

namespace compass::bench::types {

/**
 * @brief What the includes stage built and how long each step took.
 *
 * The tables come from the last repetition. roots are the source files
 * (.cpp, .cc, .cxx) that transitive fan-in counts from. Each step records
 * one wall time per repetition.
 */
struct includes_result {
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t errors = 0;
    std::string first_error;
    include_table includes;
    include_graph graph;
    std::vector<std::uint32_t> roots;
    std::vector<std::uint32_t> direct_fan_in;
    std::vector<std::uint32_t> transitive_fan_in;
    std::vector<double> scan_ms;
    std::vector<double> resolve_ms;
    std::vector<double> graph_ms;
    std::vector<double> closure_ms;
};

}

#endif

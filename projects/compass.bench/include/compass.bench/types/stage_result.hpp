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
#ifndef COMPASS_BENCH_TYPES_STAGE_RESULT_HPP
#define COMPASS_BENCH_TYPES_STAGE_RESULT_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace compass::bench::types {

/**
 * @brief What one stage did and how long each repetition took.
 *
 * The counters and checksum come from the last repetition; every repetition
 * does the same work, so they only differ if the tree changed mid-run. The
 * checksum folds every byte read, which keeps the compiler from eliding the
 * reads and lets two runs prove they saw the same content.
 */
struct stage_result {
    std::string name;
    std::uint64_t files = 0;
    std::uint64_t bytes = 0;
    std::uint64_t errors = 0;
    std::uint64_t checksum = 0;
    std::string first_error;
    std::vector<double> wall_ms;
};

}

#endif

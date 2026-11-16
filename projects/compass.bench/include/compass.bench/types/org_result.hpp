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
#ifndef COMPASS_BENCH_TYPES_ORG_RESULT_HPP
#define COMPASS_BENCH_TYPES_ORG_RESULT_HPP

#include <cstdint>
#include <vector>
#include "compass.bench/types/stage_result.hpp"

namespace compass::bench::types {

/**
 * @brief What parsing the org files of a checkout did.
 *
 * stage holds the totals and one wall time per repetition; its checksum is
 * unused. ids and parse_ms are parallel: the file id in the file table and
 * the fastest parse of that file over the repetitions. The row counts sum
 * the tables of every document of the last repetition, so two runs can
 * prove they did the same work.
 */
struct org_result {
    stage_result stage;
    std::vector<std::uint32_t> ids;
    std::vector<double> parse_ms;
    std::uint64_t elements = 0;
    std::uint64_t objects = 0;
    std::uint64_t headings = 0;
    std::uint64_t links = 0;
    std::uint64_t names = 0;
};

}

#endif

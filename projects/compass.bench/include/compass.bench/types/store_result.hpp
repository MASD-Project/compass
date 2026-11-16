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
#ifndef COMPASS_BENCH_TYPES_STORE_RESULT_HPP
#define COMPASS_BENCH_TYPES_STORE_RESULT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "compass.bench/types/stage_result.hpp"

namespace compass::bench::types {

/**
 * @brief What storing the org files of a checkout and loading them back did.
 *
 * cold holds the totals and one wall time per repetition of the cold run;
 * its bytes are source bytes and its checksum is unused. map_ms and
 * verify_ms hold one wall time per repetition of the two warm runs. objects
 * counts distinct blob IDs, which is fewer than the files when two files
 * have the same content, and object_bytes sums their sizes. rejected counts
 * the objects of the last warm repetitions that did not decode or whose
 * checksum did not match. mismatches counts the files whose loaded object
 * dumps differently from their parse, and first_mismatch names one.
 */
struct store_result {
    stage_result cold;
    std::vector<double> map_ms;
    std::vector<double> verify_ms;
    std::uint64_t objects = 0;
    std::uint64_t object_bytes = 0;
    std::uint64_t rejected = 0;
    std::uint64_t mismatches = 0;
    std::string first_mismatch;
};

}

#endif

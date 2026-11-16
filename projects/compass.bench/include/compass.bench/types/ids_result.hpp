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
#ifndef COMPASS_BENCH_TYPES_IDS_RESULT_HPP
#define COMPASS_BENCH_TYPES_IDS_RESULT_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace compass::bench::types {

/**
 * @brief What resolving the blob ID of every file of a checkout did.
 *
 * The index fields describe the index the resolver read: its version, its
 * entries, and how many of them passed the rules that do not need the
 * file's stat data. The counts come from the last repetition: files is
 * every listed file, from_index those whose ID came from the index, hashed
 * those whose bytes were hashed, hashed_bytes their size. read_ms,
 * stat_ms and hash_ms hold one wall time per repetition of the three steps:
 * building the resolver (index, configuration, attributes), stat-ing every
 * file against its entry, and hashing the rest. verified counts the IDs
 * checked against git hash-object, mismatches those that differed, and
 * first_mismatch names one with both IDs.
 */
struct ids_result {
    std::uint32_t index_version = 0;
    std::uint64_t index_entries = 0;
    std::uint64_t trusted_entries = 0;
    bool autocrlf = false;
    std::uint64_t files = 0;
    std::uint64_t from_index = 0;
    std::uint64_t hashed = 0;
    std::uint64_t hashed_bytes = 0;
    std::uint64_t errors = 0;
    std::string first_error;
    std::vector<double> read_ms;
    std::vector<double> stat_ms;
    std::vector<double> hash_ms;
    std::uint64_t verified = 0;
    std::uint64_t mismatches = 0;
    std::string first_mismatch;
};

}

#endif

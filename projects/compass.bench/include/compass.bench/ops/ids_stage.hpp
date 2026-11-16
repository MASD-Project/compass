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
#ifndef COMPASS_BENCH_OPS_IDS_STAGE_HPP
#define COMPASS_BENCH_OPS_IDS_STAGE_HPP

#include <cstdint>
#include <filesystem>
#include "compass.bench/types/file_table.hpp"
#include "compass.bench/types/ids_result.hpp"

namespace compass::bench::ops {

/**
 * @brief Resolves the blob ID of every file of the table, @p repetitions
 * times, on @p threads threads, and checks @p verify of them against
 * `git hash-object`.
 *
 * Each repetition builds a store::io::id_resolver, which reads the index,
 * then asks it for the index ID of every file, which costs one lstat each,
 * then hashes the files that got none. The three steps are timed apart.
 *
 * The check runs once, outside any timing, on the IDs of the last
 * repetition: @p verify files spread evenly over the table, or all of them
 * when @p verify is at least the table's size. Nothing is written to the
 * checkout or its git directory.
 */
[[nodiscard]] types::ids_result run_ids_stage(
    const std::filesystem::path& checkout, const types::file_table& table,
    unsigned threads, unsigned repetitions, std::uint64_t verify);

}

#endif

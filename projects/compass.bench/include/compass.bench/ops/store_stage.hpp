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
#ifndef COMPASS_BENCH_OPS_STORE_STAGE_HPP
#define COMPASS_BENCH_OPS_STORE_STAGE_HPP

#include <filesystem>
#include "compass.bench/types/file_table.hpp"
#include "compass.bench/types/store_result.hpp"

namespace compass::bench::ops {

/**
 * @brief Stores every .org file of the table as an object under @p objects
 * and loads it back, on @p threads threads.
 *
 * Three runs are timed, @p repetitions times each:
 *
 * - cold: map each source, compute its blob ID, parse it, encode the tables
 *   and write the object. The objects this run wrote are removed before each
 *   repetition, outside the timing, so every repetition writes them all.
 * - warm map: build the path of each object from its blob ID, map it and
 *   decode it, which checks the header and reads no column.
 * - warm verify: the same, then check the checksum, which reads every byte
 *   of every column.
 *
 * The warm runs take the blob IDs from the cold run; finding the ID of a
 * file without reading it is the work of the worktree index and is not
 * measured here. They read objects the cold run has just written, so the
 * page cache holds them.
 *
 * Last, outside any timing, each file is parsed and its object loaded, and
 * the two dumps are compared.
 *
 * A file that cannot be read, or an object that cannot be written, is
 * counted in the cold errors and skipped by the later runs.
 */
[[nodiscard]] types::store_result run_store_stage(
    const std::filesystem::path& checkout, const types::file_table& table,
    const std::filesystem::path& objects, unsigned threads,
    unsigned repetitions);

}

#endif

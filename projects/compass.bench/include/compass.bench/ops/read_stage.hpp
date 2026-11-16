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
#ifndef COMPASS_BENCH_OPS_READ_STAGE_HPP
#define COMPASS_BENCH_OPS_READ_STAGE_HPP

#include <filesystem>
#include <span>
#include "compass.bench/types/file_table.hpp"
#include "compass.bench/types/stage_result.hpp"

namespace compass::bench::ops {

/**
 * @brief Maps every file and folds every byte, one thread per range.
 *
 * Runs @p repetitions times and records one wall time per repetition. The
 * checksum is the wrapping sum of all bytes read, so it does not depend on
 * how the ranges were split. A file that cannot be mapped counts as an error.
 */
[[nodiscard]] types::stage_result run_read_stage(
    const std::filesystem::path& checkout, const types::file_table& table,
    std::span<const types::file_range> ranges, unsigned repetitions);

}

#endif

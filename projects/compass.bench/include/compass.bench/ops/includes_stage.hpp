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
#ifndef COMPASS_BENCH_OPS_INCLUDES_STAGE_HPP
#define COMPASS_BENCH_OPS_INCLUDES_STAGE_HPP

#include <filesystem>
#include "compass.bench/types/file_table.hpp"
#include "compass.bench/types/includes_result.hpp"
#include "compass.bench/types/search_paths.hpp"

namespace compass::bench::ops {

/**
 * @brief Scans, resolves and closes the include graph of the checkout.
 *
 * Scanning maps every file with a scanned extension on @p threads threads,
 * over ranges balanced by bytes. Resolution, graph building and transitive
 * fan-in run on one thread. Each step is timed separately on every one of
 * @p repetitions. A file that cannot be mapped counts as an error.
 */
[[nodiscard]] types::includes_result run_includes_stage(
    const std::filesystem::path& checkout, const types::file_table& table,
    const types::search_paths& paths, unsigned threads,
    unsigned repetitions);

}

#endif

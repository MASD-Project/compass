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
#ifndef COMPASS_BENCH_OPS_GC_STAGE_HPP
#define COMPASS_BENCH_OPS_GC_STAGE_HPP

#include <filesystem>
#include "compass.store/types/collection.hpp"

namespace compass::bench::ops {

/**
 * @brief Collects the store of the repository @p checkout belongs to
 * with app::collect of compass.cli, keeping every object a worktree's
 * index names and every action object a worktree's dependency record
 * names.
 */
[[nodiscard]] store::types::collection_result run_gc_stage(
    const std::filesystem::path& checkout,
    const store::types::collection_options& options);

}

#endif

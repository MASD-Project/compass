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
#ifndef COMPASS_BENCH_OPS_WARM_STAGE_HPP
#define COMPASS_BENCH_OPS_WARM_STAGE_HPP

#include <filesystem>
#include "compass.bench/types/file_table.hpp"
#include "compass.bench/types/warm_result.hpp"

namespace compass::bench::ops {

/**
 * @brief Runs the org pipeline end to end over the .org files of the
 * table, @p repetitions times, on @p threads threads.
 *
 * Each repetition builds a store::io::id_resolver and resolves the ID of
 * every file (git's index, then the worktree index, then hashing), maps
 * each file's object under @p objects and checks its header and checksum,
 * parses and writes the object of every file whose object was missing or
 * rejected, and last rewrites the worktree index of the checkout.
 *
 * Nothing is removed between repetitions, so the first fills the store
 * and the worktree index and the ones after it are warm. The checkout's
 * git directory is written to.
 */
[[nodiscard]] types::warm_result run_warm_stage(
    const std::filesystem::path& checkout, const types::file_table& table,
    const std::filesystem::path& objects, unsigned threads,
    unsigned repetitions);

}

#endif

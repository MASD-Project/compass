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
#ifndef COMPASS_BENCH_OPS_CODECS_STAGE_HPP
#define COMPASS_BENCH_OPS_CODECS_STAGE_HPP

#include <filesystem>
#include "compass.bench/types/codecs_result.hpp"
#include "compass.bench/types/file_table.hpp"
#include "compass.codec/types/configuration.hpp"

namespace compass::bench::ops {

/**
 * @brief Loads the .org files of the table as a corpus and runs the
 * enabled codecs of compass.codec over it, @p repetitions times, on
 * @p threads threads.
 *
 * The type registry comes from the contract pages of @p meta. The corpus
 * is loaded once, as the warm stage loads it: the ID of each file comes
 * from a store::io::id_resolver, its object is mapped from @p objects and
 * checked, and a file whose object is missing or rejected is parsed and
 * its object written. Sources and objects stay mapped while the codecs
 * run. The worktree index of the checkout is rewritten.
 *
 * Each repetition starts from the dependency record of the checkout, so a
 * unit whose recorded inputs are unchanged is not called, and the record
 * the last repetition made replaces the file when it differs.
 *
 * What the codecs emit is kept in the result. No output file is written
 * to the checkout; the worktree index and the dependency record under its
 * git directory are the only files written there.
 *
 * @throws std::system_error or std::filesystem::filesystem_error when the
 * meta directory or a contract page cannot be read.
 */
[[nodiscard]] types::codecs_result run_codecs_stage(
    const std::filesystem::path& checkout, const types::file_table& table,
    const std::filesystem::path& meta, const std::filesystem::path& objects,
    const codec::types::configuration& settings, unsigned threads,
    unsigned repetitions);

}

#endif

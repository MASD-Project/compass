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
#ifndef COMPASS_BENCH_OPS_TYPES_STAGE_HPP
#define COMPASS_BENCH_OPS_TYPES_STAGE_HPP

#include <filesystem>
#include "compass.bench/types/file_table.hpp"
#include "compass.bench/types/types_result.hpp"

namespace compass::bench::ops {

/**
 * @brief Builds the type registry from the document_type_*.org pages of
 * @p meta, in path order, then adds the codegen model types.
 *
 * @throws std::system_error or std::filesystem::filesystem_error when the
 * directory or a page cannot be read.
 */
[[nodiscard]] types::meta_registry build_meta_registry(
    const std::filesystem::path& meta);

/**
 * @brief Builds the registry from @p meta, then parses every .org file of
 * the table and resolves its #+type: against the registry.
 *
 * The first #+type: keyword of a document is its type. Runs @p repetitions
 * times on @p threads threads over ranges balanced by bytes. A file that
 * cannot be mapped counts as an error; a type that does not resolve is data.
 */
[[nodiscard]] types::types_result run_types_stage(
    const std::filesystem::path& checkout, const types::file_table& table,
    const std::filesystem::path& meta, unsigned threads,
    unsigned repetitions);

}

#endif

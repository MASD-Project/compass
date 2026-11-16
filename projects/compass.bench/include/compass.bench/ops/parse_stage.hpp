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
#ifndef COMPASS_BENCH_OPS_PARSE_STAGE_HPP
#define COMPASS_BENCH_OPS_PARSE_STAGE_HPP

#include <filesystem>
#include "compass.bench/types/file_table.hpp"
#include "compass.bench/types/parse_result.hpp"

namespace compass::bench::ops {

/**
 * @brief Parses every C, C++ and SQL file of the table with tree-sitter.
 *
 * Each language runs on its own, on @p threads threads over ranges balanced
 * by bytes, with one parser per thread over the mapped bytes. Every tree is
 * deleted once its error nodes are counted. Runs @p repetitions times and
 * records one wall time per language and repetition. A file that cannot be
 * mapped, or that the parser returns no tree for, counts as an error; parse
 * errors in the tree are data and do not.
 */
[[nodiscard]] types::parse_result run_parse_stage(
    const std::filesystem::path& checkout, const types::file_table& table,
    unsigned threads, unsigned repetitions);

}

#endif

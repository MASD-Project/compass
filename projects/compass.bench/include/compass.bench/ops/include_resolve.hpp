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
#ifndef COMPASS_BENCH_OPS_INCLUDE_RESOLVE_HPP
#define COMPASS_BENCH_OPS_INCLUDE_RESOLVE_HPP

#include <string>
#include <string_view>
#include "compass.bench/types/file_table.hpp"
#include "compass.bench/types/include_table.hpp"
#include "compass.bench/types/search_paths.hpp"

namespace compass::bench::ops {

/**
 * @brief Joins @p name onto @p directory, folding "." and ".." segments.
 *
 * @p directory is already normal: absolute, or relative to the checkout root
 * with "" for the root. Returns false when ".." climbs above a relative
 * root, which leaves the checkout.
 */
[[nodiscard]] bool join_normal(std::string& out, std::string_view directory,
    std::string_view name);

/**
 * @brief Fills includes.targets with one target per row.
 *
 * A quoted include tries the includer's directory, then every directory of
 * the includer's context; an angle include skips both the includer's
 * directory and the -iquote directories. The first directory holding the
 * name wins. In an internal directory the name must be a file of @p table;
 * a hit gives that file id. In an external directory it must be a regular
 * file on disk; a hit gives external_target. No hit gives
 * unresolved_target. Results are memoised per context, kind and name.
 */
void resolve_includes(types::include_table& includes,
    const types::file_table& table, const types::search_paths& paths);

}

#endif

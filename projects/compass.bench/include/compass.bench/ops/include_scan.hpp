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
#ifndef COMPASS_BENCH_OPS_INCLUDE_SCAN_HPP
#define COMPASS_BENCH_OPS_INCLUDE_SCAN_HPP

#include <cstdint>
#include <string_view>
#include "compass.bench/types/include_table.hpp"

namespace compass::bench::ops {

/**
 * @brief True for the extensions the include stage scans.
 *
 * .hpp .cpp .h .hh .cc .cxx .ipp .tpp, matched exactly.
 */
[[nodiscard]] bool is_scanned_extension(std::string_view extension);

/**
 * @brief Appends a row to @p out for every #include in @p text.
 *
 * A directive is a '#' that is the first token on its line, optionally
 * followed by blanks, then the word "include". Comments and string, char
 * and raw string literals are skipped, so directives inside them do not
 * count. Conditional compilation is not evaluated: an include under
 * "#if 0" counts. Lines are 1-based and a CR before LF is a blank.
 */
void scan_includes(std::string_view text, std::uint32_t includer,
    types::include_table& out);

}

#endif

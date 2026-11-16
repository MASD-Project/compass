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
#ifndef COMPASS_CLI_IO_REPORT_HPP
#define COMPASS_CLI_IO_REPORT_HPP

#include <ostream>
#include "compass.cli/types/results.hpp"

namespace compass::cli::io {

/**
 * @brief Writes why the run failed to @p err, one line per load error and
 * registry problem.
 */
void write_problems(std::ostream& err, const types::run_summary& s);

/**
 * @brief Writes the sink problems to @p err and the counts of what
 * generate did to @p out, in one line.
 */
void write_generate(std::ostream& out, std::ostream& err,
    const types::generate_result& r);

/**
 * @brief Writes the unified diffs to @p out, so they can be piped, and
 * the problems and counts of what generate would do to @p err.
 */
void write_diff(std::ostream& out, std::ostream& err,
    const types::diff_result& r);

/**
 * @brief Writes each finding to @p out as
 * path:line:column: rule: message (fix: fix), then their count; the
 * problems go to @p err.
 */
void write_check(std::ostream& out, std::ostream& err,
    const types::check_result& r);

/**
 * @brief Writes what gc removed, or would remove, to @p out.
 */
void write_gc(std::ostream& out, const types::gc_result& r);

}

#endif

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
#ifndef COMPASS_BENCH_IO_REPORT_HPP
#define COMPASS_BENCH_IO_REPORT_HPP

#include <ostream>
#include "compass.bench/types/file_table.hpp"
#include "compass.bench/types/stage_result.hpp"

namespace compass::bench::io {

/**
 * @brief The setup that runs once, before any stage.
 */
struct setup_timing {
    unsigned threads = 0;
    double listing_ms = 0;
    double table_ms = 0;
};

/**
 * @brief True when the stage read nothing or hit any error.
 */
[[nodiscard]] bool failed(const types::stage_result& stage);

/**
 * @brief Writes the stage table, the setup lines and the extension table.
 *
 * A failed stage gets a FAILED banner naming the reason.
 */
void write_report(std::ostream& os, const types::stage_result& stage,
    const setup_timing& setup, const types::file_table& table);

}

#endif

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
#ifndef COMPASS_CODEC_TYPES_RUN_RESULT_HPP
#define COMPASS_CODEC_TYPES_RUN_RESULT_HPP

#include <cstdint>
#include <vector>
#include "compass.codec/types/diagnostic_table.hpp"
#include "compass.codec/types/file_table.hpp"

namespace compass::codec::types {

/**
 * @brief What one run of a registry over a corpus produced, one row per
 * codec, indexed by codec_id.
 *
 * documents counts the documents the codec was given, and is zero for a
 * disabled codec, which is not called. A codec fills one of files and
 * diagnostics and leaves the other empty. Their rows are in the order one
 * thread calling the codec document by document would produce, whatever
 * the thread count. ms sums the time spent inside the codec over every
 * thread, so it can exceed wall_ms, the wall time of the whole run.
 */
struct run_result {
    std::vector<std::uint64_t> documents;
    std::vector<file_table> files;
    std::vector<diagnostic_table> diagnostics;
    std::vector<double> ms;
    double wall_ms = 0;
};

}

#endif

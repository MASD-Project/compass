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
#ifndef COMPASS_BENCH_TYPES_PARSE_RESULT_HPP
#define COMPASS_BENCH_TYPES_PARSE_RESULT_HPP

#include <array>
#include <cstdint>
#include <vector>
#include "compass.bench/types/stage_result.hpp"

namespace compass::bench::types {

/**
 * @brief The grammars the parse stage runs, usable as an array index.
 */
enum class language : std::uint8_t { cpp, sql };

inline constexpr std::size_t language_count = 2;

/**
 * @brief What parsing one language did, with one row per file.
 *
 * stage holds the totals and one wall time per repetition; its checksum is
 * unused. The columns are parallel and indexed by row: ids is the file id in
 * the file table, parse_ms the fastest parse of that file over the
 * repetitions, and error_nodes its count of ERROR and MISSING nodes. A file
 * that could not be read keeps zero in both and counts in stage.errors.
 */
struct language_result {
    stage_result stage;
    std::vector<std::uint32_t> ids;
    std::vector<double> parse_ms;
    std::vector<std::uint32_t> error_nodes;
};

/**
 * @brief The parse stage, one result per language, indexed by language.
 */
struct parse_result {
    std::array<language_result, language_count> languages;

    [[nodiscard]] const language_result& operator[](language l) const {
        return languages[static_cast<std::size_t>(l)];
    }
};

}

#endif

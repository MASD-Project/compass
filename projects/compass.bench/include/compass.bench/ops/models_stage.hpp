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
#ifndef COMPASS_BENCH_OPS_MODELS_STAGE_HPP
#define COMPASS_BENCH_OPS_MODELS_STAGE_HPP

#include <array>
#include <cstddef>
#include <filesystem>
#include <ostream>
#include <string>
#include <vector>
#include "compass.bench/types/file_table.hpp"
#include "compass.model/types/model_tables.hpp"

namespace compass::bench::ops {

inline constexpr std::size_t model_type_count =
    std::size(compass::model::types::model_types);

/**
 * @brief What loading the codegen models of a checkout did.
 *
 * documents counts the .org files of modeling/ directories under projects/
 * that were parsed, models those whose #+type: is a model type. by_type and
 * errors count models per model_type, errors those ores.codegen refuses.
 * parse_ms is the time to map and parse the documents once; load_ms holds
 * one time per repetition of loading every model from the parsed tables.
 */
struct models_result {
    std::size_t documents = 0;
    std::size_t models = 0;
    std::array<std::size_t, model_type_count> by_type{};
    std::array<std::size_t, model_type_count> errors{};
    std::size_t values = 0;
    double parse_ms = 0;
    std::vector<double> load_ms;
    std::vector<std::string> messages;
};

/**
 * @brief Loads every codegen model of the checkout @p repetitions times.
 *
 * When @p dump is not empty, writes each model of the last repetition as
 * canonical JSON to <dump>/<model path>.json, the layout
 * build/scripts/codegen_model_dump.py writes.
 */
[[nodiscard]] models_result run_models_stage(
    const std::filesystem::path& checkout, const types::file_table& table,
    unsigned repetitions, const std::filesystem::path& dump);

void write_models_report(std::ostream& os, const models_result& r);

}

#endif

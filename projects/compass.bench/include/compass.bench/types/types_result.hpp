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
#ifndef COMPASS_BENCH_TYPES_TYPES_RESULT_HPP
#define COMPASS_BENCH_TYPES_TYPES_RESULT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "compass.bench/types/stage_result.hpp"
#include "compass.ingest/meta/type_registry_builder.hpp"
#include "compass.org/types/type_registry.hpp"

namespace compass::bench::types {

/**
 * @brief The registry built from the contract pages of a meta directory,
 * with how each page fitted the extraction rule.
 *
 * paths and reports are parallel: the page path relative to the meta
 * directory and its report.
 */
struct meta_registry {
    compass::org::types::type_registry registry;
    std::vector<std::string> paths;
    std::vector<ingest::meta::contract_report> reports;
};

/**
 * @brief What resolving the #+type: of one document gave.
 */
enum class resolution : std::uint8_t { resolved, unresolved, untyped };

/**
 * @brief What building the registry and resolving every org file did.
 *
 * stage holds one wall time per repetition of parsing and resolving every
 * org file; registry_ms one time per repetition of building the registry.
 * resolve_ms sums, over every thread of the last repetition, the time spent
 * finding the #+type: keyword and looking it up. ids, outcomes, types and
 * values are parallel: the file id, the outcome, the type when resolved,
 * and the #+type: value when unresolved.
 */
struct types_result {
    stage_result stage;
    std::vector<double> registry_ms;
    double resolve_ms = 0;
    meta_registry meta;
    std::vector<std::uint32_t> ids;
    std::vector<resolution> outcomes;
    std::vector<compass::org::types::type_id> types;
    std::vector<std::string> values;
};

}

#endif

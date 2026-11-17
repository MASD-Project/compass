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
#ifndef COMPASS_CODEGEN_OPS_CONTEXT_HPP
#define COMPASS_CODEGEN_OPS_CONTEXT_HPP

#include <string_view>
#include "compass.model/ops/builder.hpp"
#include "compass.model/types/model_tables.hpp"

namespace compass::codegen::ops {

/**
 * @brief The copyright year the option year of the codec @p codec gives.
 * The year is configuration, never the clock, so a run in another year
 * writes the same bytes and a new year is one deliberate change.
 * @throws std::runtime_error naming @p codec when @p option is empty or
 * not a year.
 */
[[nodiscard]] int copyright_year(std::string_view codec,
    std::string_view option);

/**
 * @brief The values every context starts from, which core.load_data and
 * generate_from_model read from the library's data directory: the licence
 * text as licence-GPL-v3, the four modelines of @p modeline_org as
 * modelines, and per language its modeline and its licence comment block,
 * with the copyright of @p year.
 *
 * Returns an object no value holds; the contexts copy its members.
 */
compass::model::types::value_id library_values(
    compass::model::ops::builder& b, std::string_view licence,
    std::string_view modeline_org, int year);

/**
 * @brief The context generate_from_model builds for @p model, of the
 * ores.codegen model type @p model_type, read from a file whose name
 * without .org is @p model_key: the library values and the model under
 * @p model_key, and then by type:
 *
 * - component: the model's component, with the names generation derives
 *   from its name and full name, under component. The component under the
 *   model has them too, as in ores.codegen it is the one dict.
 * - asset_class_catalogue and outcome_catalogue: the catalogue under its
 *   type's name.
 *
 * No other type gets more than the model here: entity_context and the
 * contexts of model_contexts.hpp build the entities, junctions,
 * operations, field groups and families.
 */
compass::model::types::value_id model_context(
    compass::model::ops::builder& b, compass::model::types::value_id library,
    compass::model::types::value_id model, std::string_view model_key,
    std::string_view model_type);

}

#endif

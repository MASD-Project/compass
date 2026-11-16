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
#ifndef COMPASS_CODEGEN_OPS_ENTITY_REGISTRY_HPP
#define COMPASS_CODEGEN_OPS_ENTITY_REGISTRY_HPP

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codegen/types/entity_registry.hpp"
#include "compass.model/ops/outline.hpp"
#include "compass.model/types/model_tables.hpp"

namespace compass::codegen::ops {

/**
 * @brief Adds what the model @p org_text declares to @p d, by its #+type:
 * as _model_type_names reads it: an entity's #+entity_singular:, a
 * junction's #+name_singular: and a field group's first of the two.
 */
void add_directory_model(types::model_directory& d,
    std::string_view org_text);

/**
 * @brief The directory of the model at @p model_path in @p registry,
 * empty when none of its models is registered.
 */
[[nodiscard]] const types::model_directory& directory_of(
    const types::entity_registry& registry, std::string_view model_path);

/**
 * @brief _parent_entity_info of a model as loaded: @p model is its root,
 * which holds domain_entity for an entity model; any other model reads as
 * an entity that states nothing.
 */
[[nodiscard]] types::parent_entity parent_entity_of(
    const compass::model::types::model_tables& t,
    compass::model::types::value_id model);

/**
 * @brief The code and instrument of each row of the first table under the
 * * Trade types section of the trade type catalogue outlined by @p o, in
 * order, a row with no code left out: the rows trade_type_catalogue_codes
 * filters by instrument.
 */
[[nodiscard]] std::vector<types::trade_type_route> trade_type_routes_of(
    const compass::model::ops::outline& o);

/**
 * @brief _column_names: every column @p entity declares, the key columns
 * included.
 */
[[nodiscard]] std::set<std::string, std::less<>> column_names_of(
    const compass::model::types::model_tables& t,
    compass::model::types::value_id entity);

/**
 * @brief _immutable_tenant_key: whether @p entity is immutable and keys on
 * (tenant_id, primary key).
 */
[[nodiscard]] bool immutable_tenant_key_of(
    const compass::model::types::model_tables& t,
    compass::model::types::value_id entity);

/**
 * @brief The parent entity of @p table, as
 * _parent_entity_info(org_by_table.get(table).org) gives it; null when no
 * model states the table or its model does not load.
 */
[[nodiscard]] const types::parent_entity* parent_of(
    const types::entity_registry& registry, std::string_view table);

}

#endif

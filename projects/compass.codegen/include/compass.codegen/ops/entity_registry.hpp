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
#include "compass.codegen/types/entity_registry.hpp"
#include "compass.model/types/model_tables.hpp"

namespace compass::codegen::ops {

/**
 * @brief The table an org file's raw text claims for an entity, as
 * _entity_org_by_table scans it: the first #+entity_singular: at the start
 * of a line and the first ":tablename: <name>_tbl".
 */
struct entity_table_claim {
    std::string table;
    std::string entity_singular;
};

/**
 * @brief The claim of @p org_text, or none when it states no entity
 * singular or no table name.
 */
[[nodiscard]] std::optional<entity_table_claim> entity_table_claim_of(
    std::string_view org_text);

/**
 * @brief _parent_entity_info of a model as loaded: @p model is its root,
 * which holds domain_entity for an entity model; any other model reads as
 * an entity that states nothing.
 */
[[nodiscard]] types::parent_entity parent_entity_of(
    const compass::model::types::model_tables& t,
    compass::model::types::value_id model);

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

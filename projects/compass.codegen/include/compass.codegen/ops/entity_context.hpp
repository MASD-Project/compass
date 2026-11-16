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
#ifndef COMPASS_CODEGEN_OPS_ENTITY_CONTEXT_HPP
#define COMPASS_CODEGEN_OPS_ENTITY_CONTEXT_HPP

#include <string_view>
#include "compass.codegen/types/entity_registry.hpp"
#include "compass.model/ops/builder.hpp"
#include "compass.model/types/model_tables.hpp"

namespace compass::codegen::ops {

/**
 * @brief The context generate_from_model builds for the domain entity
 * @p model, read from a file whose name without .org is @p model_key, for
 * every template whose context does not depend on the template: the
 * library values, the model under @p model_key, and the entity, enriched
 * step by step in the order generate_from_model applies the steps, under
 * domain_entity, with its SQL projection under table.
 *
 * @p registry holds what the steps read from the checkout's other models.
 *
 * Returns an object no value holds.
 */
compass::model::types::value_id entity_context(
    compass::model::ops::builder& b, compass::model::types::value_id library,
    compass::model::types::value_id model, std::string_view model_key,
    const types::entity_registry& registry);

}

#endif

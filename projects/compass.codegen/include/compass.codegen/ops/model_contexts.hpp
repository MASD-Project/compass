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
#ifndef COMPASS_CODEGEN_OPS_MODEL_CONTEXTS_HPP
#define COMPASS_CODEGEN_OPS_MODEL_CONTEXTS_HPP

#include <string_view>
#include "compass.codegen/types/entity_registry.hpp"
#include "compass.model/ops/builder.hpp"
#include "compass.model/types/model_tables.hpp"

/**
 * @brief The contexts generate_from_model builds for the model types that
 * sit beside a domain entity: junctions, operations and field groups. Each
 * holds the library values and the model under @p model_key, and its
 * enriched member, the very object under the model, again at the top under
 * the type's name. @p registry holds what the enrichment reads from the
 * checkout's other models, @p site what it reads about the model's file.
 * Each returns an object no value holds, which is the context of every
 * template of the model that reads no more; template_context builds the
 * others from it.
 */
namespace compass::codegen::ops {

/**
 * @brief A junction's shared context and, apart from it, the shape its
 * shell and HTTP route templates read under domain_entity.
 */
struct junction_contexts {
    compass::model::types::value_id context;
    compass::model::types::value_id surface;
};

/**
 * @brief The junction block of generate_from_model (core.py 6865-7204):
 * the junction's columns and sides flagged, its history and events when
 * it states them, its protocol messages and operations derived from
 * junction_entity_shape, its names, include gates and tenant scope. The
 * surface is junction_entity_shape with the messages, operations, write
 * record and surface plans, built where the Python builds it, before the
 * repository section is copied onto the junction; none when the model
 * has no junction.
 */
junction_contexts junction_context(
    compass::model::ops::builder& b, compass::model::types::value_id library,
    compass::model::types::value_id model, std::string_view model_key,
    const types::entity_registry& registry, const types::entity_site& site);

/**
 * @brief The operation block of generate_from_model (core.py 7258-7312),
 * less what only one template reads: the component path names and their
 * upper-case spellings, and _project_wire_codec.
 */
compass::model::types::value_id operation_context(
    compass::model::ops::builder& b, compass::model::types::value_id library,
    compass::model::types::value_id model, std::string_view model_key,
    const types::entity_registry& registry, const types::entity_site& site);

/**
 * @brief The field-group block of generate_from_model (core.py
 * 7206-7256): the description as comment lines, the include names, the
 * wire codec facts, the last field marked and the TypeScript names.
 */
compass::model::types::value_id field_group_context(
    compass::model::ops::builder& b, compass::model::types::value_id library,
    compass::model::types::value_id model, std::string_view model_key,
    const types::entity_registry& registry, const types::entity_site& site);

}

#endif

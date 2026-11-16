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
#ifndef COMPASS_CODEGEN_OPS_MODEL_STEPS_HPP
#define COMPASS_CODEGEN_OPS_MODEL_STEPS_HPP

#include <string>
#include <string_view>
#include "compass.codegen/ops/values.hpp"
#include "compass.codegen/types/entity_registry.hpp"
#include "compass.model/ops/builder.hpp"

/**
 * @brief What the junction, operation and field-group contexts share.
 */
namespace compass::codegen::ops {

/**
 * @brief A context being built: the context object and the member of the
 * model under its key that the enrichment changes in place, none when the
 * model has no such object.
 */
struct context_base {
    compass::model::types::value_id context;
    compass::model::types::value_id owned;
};

/**
 * @brief A new context holding the members of @p library and @p model
 * under @p model_key, with the object @p member of that copy.
 */
inline context_base base_context(compass::model::ops::builder& b,
    compass::model::types::value_id library,
    compass::model::types::value_id model, std::string_view model_key,
    std::string_view member) {
    const auto context = b.detached_object();
    for (const auto c : values::items(b.tables(), library))
        b.set_copy(context, std::string(values::key_of(b.tables(), c)), c);
    b.set_copy(context, model_key, model);
    const auto owned = values::member(b.tables(),
        b.find(context, model_key), member);
    if (!values::is(b.tables(), owned, values::value_kind::object))
        return {context, compass::model::types::value_id::none()};
    return {context, owned};
}

/**
 * @brief model.update(_component_path_vars(model)): component and the
 * directory, include, core, service, cache and generator-facet names
 * derived from component, subcomponent and cached_by.
 */
void set_component_path_vars(compass::model::ops::builder& b,
    compass::model::types::value_id model);

/**
 * @brief _project_wire_codec on an operation or family @p model: whether
 * the model at @p site states its own wire shape, the codec headers its
 * messages' member types are declared in when it does, and each
 * message's name again as struct_name.
 */
void project_wire_codec(compass::model::ops::builder& b,
    compass::model::types::value_id model,
    const types::entity_registry& registry, const types::entity_site& site);

}

#endif

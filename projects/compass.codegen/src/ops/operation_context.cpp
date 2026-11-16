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
#include "compass.codegen/ops/model_contexts.hpp"

#include <string>
#include <vector>
#include "compass.codegen/ops/entity_steps.hpp"
#include "compass.codegen/ops/values.hpp"
#include "entity_text.hpp"
#include "model_steps.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::value_id;

value_id operation_context(builder& b, value_id library, value_id model,
    std::string_view model_key, const types::entity_registry& registry,
    const types::entity_site& site) {
    const auto [context, operation] = base_context(b, library, model,
        model_key, "operation");
    if (operation.is_none())
        return context;
    set_component_path_vars(b, operation);
    for (const auto key : {"component", "component_include", "component_core",
             "component_service"})
        b.set(operation, std::string(key) + "_upper", text::upper_snake(
            values::str(b.tables(), operation, key)));
    if (!values::member(b.tables(), operation, "entity_singular").is_none())
        b.set(operation, "entity_singular_upper", text::upper(
            values::str(b.tables(), operation, "entity_singular")));
    project_wire_codec(b, operation, registry, site);
    b.set_copy(context, "operation", operation);
    return context;
}

}

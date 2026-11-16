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
#ifndef COMPASS_CODEGEN_OPS_TEMPLATE_CONTEXT_HPP
#define COMPASS_CODEGEN_OPS_TEMPLATE_CONTEXT_HPP

#include <optional>
#include <string>
#include <string_view>
#include "compass.codegen/types/entity_registry.hpp"
#include "compass.model/ops/builder.hpp"
#include "compass.model/types/model_tables.hpp"

/**
 * @brief The parts of generate_from_model's context that depend on the
 * template: a model's context is built once and shared by its templates,
 * and the few templates that read more get a copy of it with that more.
 */
namespace compass::codegen::ops {

/**
 * @brief recipe_org_id: the uuid5 of @p name in the recipe namespace,
 * upper case.
 */
[[nodiscard]] std::string recipe_org_id(std::string_view name);

/**
 * @brief convention_family_registry's _entry_for: the family the model at
 * @p path with text @p text declares with #+convention_family:, when it is
 * an entity model that does.
 */
[[nodiscard]] std::optional<types::convention_family> convention_family_of(
    std::string_view path, std::string_view text);

/**
 * @brief The context @p template_name renders from, when it reads more
 * than @p context, the shared context of a model of type @p model_type
 * under @p model_key: a copy of @p context with shell_recipe
 * (shell_recipe_document), http_recipe (http_recipe_document),
 * convention_unit or convention_registry (convention_family_context) at
 * the top, or with the entity's utility_imports and extra_domain_imports
 * for ts_protocol.ts.mustache. None for every other template.
 */
[[nodiscard]] compass::model::types::value_id template_context(
    compass::model::ops::builder& b, compass::model::types::value_id context,
    std::string_view model_key, std::string_view model_type,
    std::string_view template_name, const types::entity_registry& registry);

}

#endif

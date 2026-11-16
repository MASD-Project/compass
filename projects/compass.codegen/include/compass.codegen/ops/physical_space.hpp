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
#ifndef COMPASS_CODEGEN_OPS_PHYSICAL_SPACE_HPP
#define COMPASS_CODEGEN_OPS_PHYSICAL_SPACE_HPP

#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codegen/types/physical_space.hpp"
#include "compass.model/types/model_tables.hpp"

namespace compass::codegen::ops {

/**
 * @brief One ores.*.org node document of the template library: its file
 * name and its text.
 */
struct node_document {
    std::string_view name;
    std::string_view text;
};

/**
 * @brief The graph physical_space.load_graph builds from @p nodes, which
 * must be in file-name order: each node's #+ keywords up to its first
 * heading, and the template its :tangle names.
 */
[[nodiscard]] types::physical_space load_physical_space(
    std::span<const node_document> nodes);

/**
 * @brief A property of a model document's file-level drawer.
 */
struct drawer_property {
    std::string_view key;
    std::string_view value;
};

/**
 * @brief _facet_enabled: whether every one of @p addresses of @p facet is
 * enabled under the ores.cpp tech space, after the
 * :ores.<address>.enabled: entries of @p properties, the most specific
 * address first, @p fallback when none speaks.
 */
[[nodiscard]] bool facet_enabled(std::span<const drawer_property> properties,
    std::string_view facet, std::span<const std::string_view> addresses,
    bool fallback);

/**
 * @brief _protocol_owned_by_operation: whether an operation model in the
 * directory of @p model_path, or the one above it, states the protocol of
 * the domain entity of @p model, by its component and singular.
 */
[[nodiscard]] bool protocol_owned(const types::checkout_registry& registry,
    const compass::model::types::model_tables& models,
    compass::model::types::value_id model, std::string_view model_path);

/**
 * @brief One output to render: the template and the output path relative
 * to the root of the checkout.
 */
struct unit {
    std::string template_name;
    std::string output;
};

/**
 * @brief A file of the checkout: its path relative to the root of the
 * checkout and its text.
 */
struct checkout_file {
    std::string_view path;
    std::string_view text;
};

/**
 * @brief The registry resolve_targets reads from @p files, which should be
 * every .org file under a modeling/ directory of projects/: the operation
 * models (by #+type:), the plural each projects/<component>/modeling/
 * document states first (#+entity_plural: or #+name:), and the
 * #+shell_command_aggregator: of each
 * projects/ores.shell/<component>/modeling/component_overview.org.
 */
[[nodiscard]] types::checkout_registry read_checkout_registry(
    std::span<const checkout_file> files);

/**
 * @brief What generate.resolve_targets returns, with no --address, for a
 * model of @p model_type, which is ores.codegen's name for it (component,
 * domain_entity, ...): the archetypes the graph enables for it, in its
 * order, each with its output path, an output named twice kept once.
 *
 * Output paths are resolved as resolve_output_path resolves them for a
 * component and for a domain entity. A component's directory under
 * projects/ is the one above the modeling/ of @p model_path, the model's
 * path relative to the checkout. A domain entity is narrowed first by the
 * gates resolve_targets applies to one: what @p registry says owns its
 * protocol, its menu and its shell registrar, its audit shape and its
 * profile. An archetype whose path names a placeholder the entity does
 * not declare is skipped. Any other model type's pattern is kept as
 * written, which is right only for the types whose patterns name no
 * placeholder, such as the catalogues. @p properties is the model's
 * drawer followed by its physical_space_overrides, a later key winning;
 * their :ores.<address>.enabled: entries override the defaults, the most
 * specific address first.
 */
[[nodiscard]] std::vector<unit> model_units(
    const types::physical_space& graph,
    const types::checkout_registry& registry, std::string_view model_type,
    const compass::model::types::model_tables& models,
    compass::model::types::value_id model, std::string_view model_path,
    std::span<const drawer_property> properties);

}

#endif

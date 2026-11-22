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
#ifndef COMPASS_CODEGEN_TYPES_PHYSICAL_SPACE_HPP
#define COMPASS_CODEGEN_TYPES_PHYSICAL_SPACE_HPP

#include <functional>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "compass.codegen/types/recorded.hpp"

namespace compass::codegen::types {

/**
 * @brief The facets of the physical-space graph, one row each, in the
 * order of their node files.
 *
 * model_types holds the #+model_types: words; none means every model type.
 * defaults is the facet's #+default:, true unless it says it is off.
 */
struct facet_table {
    std::vector<std::string> addresses;
    std::vector<std::string> technical_spaces;
    std::vector<std::vector<std::string>> model_types;
    std::vector<char> defaults;

    [[nodiscard]] std::size_t size() const noexcept {
        return addresses.size();
    }
};

/**
 * @brief The archetypes of the graph, one row each, in the order of their
 * node files.
 *
 * template_names is the .mustache file the node tangles, outputs the
 * #+output: pattern. model_types is the archetype's own list, or its
 * facet's when it states none. kinds lists the component kinds it serves,
 * none meaning every kind. defaults resolves #+default: against the
 * facet's.
 */
struct archetype_table {
    std::vector<std::string> addresses;
    std::vector<std::string> facets;
    std::vector<std::string> template_names;
    std::vector<std::string> outputs;
    std::vector<std::vector<std::string>> model_types;
    std::vector<std::vector<std::string>> kinds;
    std::vector<std::string> data_sources;
    std::vector<std::string> requires_facets;
    std::vector<char> defaults;

    [[nodiscard]] std::size_t size() const noexcept {
        return addresses.size();
    }
};

/**
 * @brief ores.codegen's physical-space graph: the facets and archetypes the
 * <root>.*.org node documents of the template library declare in their
 * headers. root is the first part of every address, the project's
 * product.
 */
struct physical_space {
    std::string root;
    facet_table facets;
    archetype_table archetypes;
};

/**
 * @brief What resolve_targets reads from the checkout around a model,
 * beyond the graph and the model itself, read once for every model.
 *
 * protocol_owners holds, for each operation model, (directory, component,
 * entity_singular) for the directory it lives in and the one above it,
 * the two levels _operation_protocol_owners scans. plural_owners maps each
 * plural a projects/<component>/modeling/ document names to those
 * components, as _plural_owners reads them. shell_registrars maps the
 * components whose shell part opts into the command registrar to the
 * local units it names (shell_command_surface).
 *
 * Every collection is recorded, as those of the entity registry are.
 */
struct checkout_registry {
    recorded<std::set<std::tuple<std::string, std::string, std::string>>>
        protocol_owners{"protocol_owner"};
    recorded<std::map<std::string, std::set<std::string>, std::less<>>>
        plural_owners{"plural_owners"};
    recorded<std::map<std::string, std::vector<std::string>, std::less<>>>
        shell_registrars{"shell_registrars"};

    void record_to(fact_reads* reads) noexcept {
        protocol_owners.record_to(reads);
        plural_owners.record_to(reads);
        shell_registrars.record_to(reads);
    }
};

}

#endif

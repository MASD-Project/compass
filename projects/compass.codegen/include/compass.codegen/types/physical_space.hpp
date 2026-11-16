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

#include <string>
#include <vector>

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
 * ores.*.org node documents of the template library declare in their
 * headers.
 */
struct physical_space {
    facet_table facets;
    archetype_table archetypes;
};

}

#endif

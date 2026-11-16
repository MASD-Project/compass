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
#ifndef COMPASS_CODEGEN_OPS_MANIFEST_HPP
#define COMPASS_CODEGEN_OPS_MANIFEST_HPP

#include <functional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace compass::codegen::ops {

/**
 * @brief A row of the * Components table of ores.codegen's
 * library/component_catalogue.org: a component, the modeling/ directory its
 * models are discovered in, and the model types it skips.
 */
struct catalogue_component {
    std::string name;
    std::string modeling_dir;
    std::vector<std::string> excluded_types;
};

/**
 * @brief The components of the catalogue @p org, in its order, as
 * manifest._load_components reads them.
 */
[[nodiscard]] std::vector<catalogue_component> read_component_catalogue(
    std::string_view org);

/**
 * @brief The models `codegen.sh regenerate --all` generates, as paths
 * relative to the checkout: per component, each .org file directly in its
 * modeling directory whose #+type: is a codegen model type it does not
 * exclude, and the component_overview.org of each part beside that
 * directory.
 *
 * @p paths are the files of the checkout; @p type_of gives the #+type: of
 * one of them.
 */
[[nodiscard]] std::set<std::string, std::less<>> regenerated_models(
    std::span<const catalogue_component> components,
    std::span<const std::string_view> paths,
    const std::function<std::string(std::string_view path)>& type_of);

}

#endif

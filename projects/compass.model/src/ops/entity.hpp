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
#ifndef COMPASS_MODEL_OPS_ENTITY_HPP
#define COMPASS_MODEL_OPS_ENTITY_HPP

#include <string>
#include <string_view>
#include <vector>
#include "loaders.hpp"

/*
 * What the entity loader shares with the junction loader and its own
 * checks.
 */
namespace compass::model::ops {

/**
 * @brief One literal row of a profile's Assignments table: a boolean or a
 * text default for a feature.
 */
struct profile_assignment {
    std::string feature;
    bool is_bool = false;
    bool flag = false;
    std::string text;
};

/**
 * @brief _load_profile_assignments: the literal rows of
 * projects/modeling/variability_<slug>.org, read through the finder.
 */
[[nodiscard]] std::vector<profile_assignment> profile_assignments(
    load_context& c, std::string_view slug);

/**
 * @brief _parse_profile_list over the value of :profile:.
 */
[[nodiscard]] std::vector<std::string> profile_list(const builder& b,
    types::value_id profile);

/**
 * @brief _parse_presentation_drawer into @p out, seeded with the
 * presentation defaults of the profiles @p profile names.
 */
void presentation_drawer(load_context& c, types::value_id out, node_id drawer,
    types::value_id profile);

/**
 * @brief _resolve_domain_group_fields: the fields each domain group's field
 * group model declares, and the refusals that keep a grouped struct whole.
 */
void resolve_domain_group_fields(load_context& c, types::value_id de);

/**
 * @brief validate_model: refuses an entity it finds errors in.
 */
void validate_entity(load_context& c, types::value_id de);

}

#endif

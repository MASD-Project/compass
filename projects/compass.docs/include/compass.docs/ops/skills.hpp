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
#ifndef COMPASS_DOCS_OPS_SKILLS_HPP
#define COMPASS_DOCS_OPS_SKILLS_HPP

#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"

namespace compass::docs::ops {

/**
 * @brief Where ORE Studio keeps its skills, one directory each.
 */
inline constexpr std::string_view skills_dir = "doc/llm/skills/";

/**
 * @brief The names of the skill directories, those directly under
 * skills_dir that hold a SKILL.org, sorted by name.
 */
[[nodiscard]] std::vector<std::string> skill_names(
    const compass::codec::types::corpus_view& v);

/**
 * @brief The names of the skill directories directly under @p directory,
 * which ends with a slash, that hold a SKILL.org, sorted by name.
 */
[[nodiscard]] std::vector<std::string> skill_names(
    const compass::codec::types::corpus_view& v, std::string_view directory);

}

#endif

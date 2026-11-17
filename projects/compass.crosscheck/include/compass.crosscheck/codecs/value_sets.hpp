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
#ifndef COMPASS_CROSSCHECK_CODECS_VALUE_SETS_HPP
#define COMPASS_CROSSCHECK_CODECS_VALUE_SETS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's ore-parameter-value-sets and ore-element-map
 * generators (generate_value_sets.py, generate_ore_element_map.py) as one
 * codec: the value set seed, the element map and its columns, from one
 * resolution of the value sets. Writes nothing where the value sets have a
 * problem, which parameter_value_sets reports.
 */
extern const compass::codec::types::descriptor ore_value_sets;

/**
 * @brief ORE Studio's parameter-value-sets-only gate
 * (check_parameter_value_sets.py), with the problems the two generators
 * stop on: every rule the value sets must keep, from the same resolution.
 */
extern const compass::codec::types::descriptor parameter_value_sets;

}

#endif

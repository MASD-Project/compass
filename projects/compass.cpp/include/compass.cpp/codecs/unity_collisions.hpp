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
#ifndef COMPASS_CPP_CODECS_UNITY_COLLISIONS_HPP
#define COMPASS_CPP_CODECS_UNITY_COLLISIONS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's unity-collisions gate: no two sources of a target
 * define the same file-local name.
 *
 * Reads every .cpp under projects/ores.*, outside build and node_modules,
 * with comments and literals blanked, and collects each name with internal
 * linkage at namespace scope: in an anonymous namespace, static, or a
 * const or constexpr variable that is not inline. Rule
 * unrecorded_collision: two sources of one target, the directory whose
 * CMakeLists.txt is nearest, define the same name in the same named
 * namespace, and cmake/unity_exclusions.cmake keeps at most one of them
 * out of the unity build. Rule missing_recorded_file: the exclusions name
 * a file that does not exist.
 */
extern const compass::codec::types::descriptor unity_collisions;

}

#endif

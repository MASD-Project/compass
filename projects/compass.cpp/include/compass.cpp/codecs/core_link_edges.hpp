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
#ifndef COMPASS_CPP_CODECS_CORE_LINK_EDGES_HPP
#define COMPASS_CPP_CODECS_CORE_LINK_EDGES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's core-link-edges gate: no core library links another
 * component's core or service.
 *
 * Reads every CMakeLists.txt under projects/ without its comments. A file
 * that sets its name to ores.<component>.core declares the libraries its
 * target_link_libraries calls on <name>.lib name, with the CMake variables
 * the generated files use resolved. Rule forbidden_edge: a core links
 * ores.<other>.core.lib or ores.<other>.service.lib of another component
 * the tree declares. Rule unresolved_link: a linked name still holds a
 * variable. Rule unnamed_core: a
 * projects/<component>/core/src/CMakeLists.txt states no name. The
 * script's JSON baseline is not read: each accepted edge carries an inline
 * exception above its line in the CMake file, and an exception left once
 * its link goes is reported stale.
 */
extern const compass::codec::types::descriptor core_link_edges;

}

#endif

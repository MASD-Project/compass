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
#ifndef COMPASS_DOCS_CODECS_PHYSICAL_INVENTORY_HPP
#define COMPASS_DOCS_CODECS_PHYSICAL_INVENTORY_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's physical-inventory generator
 * (regenerate_physical_space_inventories.py): the child tables of the
 * physical-space documents in projects/ores.codegen/library/templates.
 *
 * The root, the technical_space titled ores, lists the technical spaces
 * titled ores.*; each of those lists its facets, those whose
 * #+facet_group names it; each facet titled ores.* lists its archetypes,
 * those whose #+facet names it. A table replaces the first table of its
 * section, or is appended to it; a missing section is created before
 * See also, or at the end. Every document that gets a table is emitted.
 */
extern const compass::codec::types::descriptor physical_inventory;

}

#endif

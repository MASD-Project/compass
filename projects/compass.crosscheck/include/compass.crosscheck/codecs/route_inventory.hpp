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
#ifndef COMPASS_CROSSCHECK_CODECS_ROUTE_INVENTORY_HPP
#define COMPASS_CROSSCHECK_CODECS_ROUTE_INVENTORY_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief The HTTP route inventory: http_route_inventory.org, every route
 * the BFF and ores.http register, decided by the rules and exemptions of
 * the check document whose Definition names this codec, and paired by its
 * pairs and families, with the area map, the composed views and the
 * coverage. The document gives the page's head, the kind of a composed
 * view and the area of a route that publishes none; a missing table or
 * column, or a row that does not read, is reported as unreadable and
 * writes nothing.
 *
 * The inventory was written by ORE Studio's route_inventory.py --write,
 * whose --check also failed on a registration it could not read, a
 * decision with no published kind, area or note, and a pair naming a
 * route neither server answers; those judge sources, not the page, and
 * this codec does not report them.
 */
extern const compass::codec::types::descriptor route_inventory;

}

#endif

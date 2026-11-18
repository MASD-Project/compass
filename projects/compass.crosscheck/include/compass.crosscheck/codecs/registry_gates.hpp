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
#ifndef COMPASS_CROSSCHECK_CODECS_REGISTRY_GATES_HPP
#define COMPASS_CROSSCHECK_CODECS_REGISTRY_GATES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's route-units gate (check_route_units.py): every route
 * unit a component opts into is registered by its aggregate.
 *
 * For each directory of projects/ores.http with an aggregate
 * src/routes/<c>/<c>_routes.cpp, the models of projects/ores.<c>/modeling
 * that opt in with :ores.cpp.http-route.enabled: true own a unit named by
 * their singular. Rules: unregistered_unit, twice_registered, and
 * missing_unit for a registered unit with no source.
 */
extern const compass::codec::types::descriptor route_units;

/**
 * @brief ORE Studio's registry-exceptions gate
 * (check_registry_exceptions.py): every accepted exception of the
 * component registry is justified.
 *
 * Reads COMPONENTS_UNDER_TEST and ACCEPTED_EXCEPTIONS of the registry
 * module as Python literals and the item IDs of the clean standard's
 * checklist rows. Rule bad_exception: an exception of an unlisted
 * component, of an item the standard does not define or recorded twice,
 * or with no reason, no acceptor or no ISO date. Rule unreadable_registry:
 * no item, or a table that is not a literal.
 */
extern const compass::codec::types::descriptor registry_exceptions;

}

#endif

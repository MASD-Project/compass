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
#ifndef COMPASS_SQL_CODECS_POPULATE_REFS_HPP
#define COMPASS_SQL_CODECS_POPULATE_REFS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::sql::codecs {

/**
 * @brief ORE Studio's populate-refs gate: every name a populate script
 * looks up is one a populate script that psql runs before it defines.
 *
 * A scan of every .sql under projects/ores.sql. The fact of a file is its
 * includes; for a create script, each function with its parameters, the
 * mandatory lookups its body guards with a raise of not found, and the
 * rows it inserts; for a populate script, each call of a *_fn with a
 * literal argument. The reduce joins them, in the order
 * ops::walk_includes gives. Rule unresolved_reference: a call passes
 * literal names to a lookup that no call to a defining function passes,
 * or that only calls psql runs later pass. Rule unbalanced_parentheses: a
 * definition or call the check reads does not close. Rule
 * unread_function: a function a lookup involves is not named *_fn, so its
 * calls are not read.
 */
extern const compass::codec::types::descriptor populate_refs;

}

#endif

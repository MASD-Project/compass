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
 * looks up is one a populate script defines.
 *
 * Reads the create scripts under projects/ores.sql/create for each
 * function, its parameters, the mandatory lookups its body guards with a
 * raise of not found, and the rows it inserts; then the populate scripts
 * under projects/ores.sql/populate for every call to a function that looks
 * a name up or writes a row another looks up. Rule unresolved_reference: a
 * call passes literal names to a lookup that no call to a defining
 * function passes. Rule unbalanced_parentheses: a definition or call the
 * check reads does not close.
 */
extern const compass::codec::types::descriptor populate_refs;

}

#endif

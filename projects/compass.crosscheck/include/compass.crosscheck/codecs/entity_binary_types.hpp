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
#ifndef COMPASS_CROSSCHECK_CODECS_ENTITY_BINARY_TYPES_HPP
#define COMPASS_CROSSCHECK_CODECS_ENTITY_BINARY_TYPES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's entity-binary-types gate: a generated row binds
 * each column in the binary format its column's type defines.
 *
 * Reads every generated repository/<name>_entity.hpp with a columns() list, its
 * tablename, the members of its row struct and its bind and key bindings,
 * and the CREATE TABLE and CREATE VIEW statements of the create scripts one
 * directory under projects/ores.sql/create/. Rule wire_mismatch: a binding
 * whose member's C++ type and column's SQL type travel in different
 * formats, or that names a column or member that does not exist. Rule
 * unclassified: a type the check cannot place, unless both are text. Rule
 * no_table: no create script creates the row's table. Rule
 * too_few_compared: fewer than min_compared binary columns compared.
 */
extern const compass::codec::types::descriptor entity_binary_types;

}

#endif

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
 * A document codec over the entity and junction models directly under a
 * modeling/ directory of projects/ whose repository row and create script
 * code generation renders, each read as code generation reads it
 * (ops::generated_shape_of): every column the model states that the row
 * binds with key or bind, with the member type the row's template gives
 * it and the type the create script gives it. Rule wire_mismatch: the two
 * travel in different formats, or the row declares no member for a key
 * column. Rule unclassified: a type the check cannot place, unless the
 * member is text. Both are reported on the column's heading.
 */
extern const compass::codec::types::descriptor entity_binary_types;

}

#endif

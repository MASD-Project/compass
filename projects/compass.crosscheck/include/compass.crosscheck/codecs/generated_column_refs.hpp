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
#ifndef COMPASS_CROSSCHECK_CODECS_GENERATED_COLUMN_REFS_HPP
#define COMPASS_CROSSCHECK_CODECS_GENERATED_COLUMN_REFS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's generated-column-refs gate: what a model's free-text
 * sections hand to generated code names only columns its entity has.
 *
 * A document codec over the entity and junction models directly under a
 * modeling/ directory of projects/, each read as code generation reads it
 * (ops::generated_shape_of), so an edit to a model checks that model.
 * Rule not_a_member: a column of the model's Table display that is not a
 * member of the domain struct the model generates, a field of a group
 * reached through its member, when the table writer is generated. Rule
 * not_a_column: a column an index of the model names, outside an
 * expression, that the create script the model generates does not make,
 * when the script is generated. Both are reported on the model's heading.
 */
extern const compass::codec::types::descriptor generated_column_refs;

}

#endif

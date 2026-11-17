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
 * @brief ORE Studio's generated-column-refs gate: generated code names
 * no column its entity does not have.
 *
 * Rule not_a_member: a generated projects/<component>/api/src/domain/
 * <entity>_table.cpp streams, through its loop variable, a name the entity's
 * one header under include/<name>/domain/ does not hold, comments aside.
 * Rule not_a_column: a generated create script indexes a column its CREATE
 * TABLE does not declare, or a table it does not create.
 */
extern const compass::codec::types::descriptor generated_column_refs;

}

#endif

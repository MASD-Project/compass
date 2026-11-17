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
#ifndef COMPASS_CROSSCHECK_CODECS_ORE_RENDERINGS_TABLES_HPP
#define COMPASS_CROSSCHECK_CODECS_ORE_RENDERINGS_TABLES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's ore-renderings-tables generator
 * (generate_renderings_tables.py): the reader's code tables,
 * projects/ores.ore/core/src/domain/conventions_renderings.hpp, from the
 * tables of projects/ores.sql/utility/ore_renderings.json.
 *
 * Each table holds one row per rendering, mapping it to its canonical
 * form, in the order the file states them. The script's --check also fails
 * when conventions_fields.cpp states a code table of its own; that is a
 * verdict on a source, not a generated file, and this codec does not
 * report it.
 */
extern const compass::codec::types::descriptor ore_renderings_tables;

}

#endif

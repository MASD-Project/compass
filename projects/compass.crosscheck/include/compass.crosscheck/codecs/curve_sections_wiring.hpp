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
#ifndef COMPASS_CROSSCHECK_CODECS_CURVE_SECTIONS_WIRING_HPP
#define COMPASS_CROSSCHECK_CODECS_CURVE_SECTIONS_WIRING_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's curve-sections-wiring generator
 * (generate_curve_section_parameters.py): the curve screen's column wiring,
 * curve-sections.generated.ts, and the curve part validators,
 * refdata_curve_configuration_parts_validate_fn_create.sql, from
 * curve_configuration_projection.cpp and curveconfig.xsd.
 *
 * Where the script reports a problem and writes nothing, the codec writes
 * nothing.
 */
extern const compass::codec::types::descriptor curve_sections_wiring;

/**
 * @brief ORE Studio's curve-column-guards generator
 * (generate_curve_column_guards.py): the curve configuration column map, its
 * value guard and the guard's drop, from the same wiring and each family
 * entity's :tablename:.
 */
extern const compass::codec::types::descriptor curve_column_guards;

}

#endif

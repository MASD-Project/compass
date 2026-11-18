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
 * @brief ORE Studio's two curve generators over one wiring of
 * curve_configuration_projection.cpp and curveconfig.xsd.
 *
 * generate_curve_section_parameters.py: the curve screen's column wiring,
 * curve-sections.generated.ts; the curve part validators,
 * refdata_curve_configuration_parts_validate_fn_create.sql; and the
 * gateway's wiring, curve_section_wiring.hpp and curve_section_wiring.cpp
 * of ores.http's refdata routes, which go through clang-format, the
 * formatter the options clang_format and clang_format_style name.
 *
 * generate_curve_column_guards.py: the curve configuration column map,
 * its value guard and the guard's drop, from the same wiring and each
 * family entity's :tablename:.
 *
 * Where a script reports a problem and writes nothing, the codec refuses
 * the outputs it cannot write and says why.
 */
extern const compass::codec::types::descriptor curve_sections_wiring;

}

#endif

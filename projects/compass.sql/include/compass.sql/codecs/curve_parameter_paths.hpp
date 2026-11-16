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
#ifndef COMPASS_SQL_CODECS_CURVE_PARAMETER_PATHS_HPP
#define COMPASS_SQL_CODECS_CURVE_PARAMETER_PATHS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::sql::codecs {

/**
 * @brief ORE Studio's curve-parameter-paths gate: every curve parameter
 * path is one the ORE schema declares.
 *
 * Reads the complex types of the schema option and the schemas it
 * includes, and the quoted /CurveConfiguration/ paths of the artefact
 * option, and walks each path from the type of the CurveConfiguration
 * element. Rule undeclared_path: a segment its parent type does not
 * declare, unless an exemption names the parent path and the element. Rule
 * stale_exemption: an exemption no path of the artefact needs. Rule
 * unreadable_input: the schema declares no CurveConfiguration element. The
 * exemptions file stays: a rule is a pattern over paths with * segments,
 * not a line, so it cannot become an inline exception.
 */
extern const compass::codec::types::descriptor curve_parameter_paths;

}

#endif

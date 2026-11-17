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
#ifndef COMPASS_CROSSCHECK_CODECS_PROTOCOL_TWINS_HPP
#define COMPASS_CROSSCHECK_CODECS_PROTOCOL_TWINS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's protocol-twins gate (check_protocol_twin_coverage.py):
 * every generated protocol header has its TypeScript twin.
 *
 * The twin's directory comes from each ores.ts archetype of the template
 * library's physical-space graph whose output ends in _protocol.ts and names
 * {component}. A component of the codegen catalogue that
 * COMPONENTS_UNDER_TEST lists, under its name or without a -cpp suffix, and
 * whose twin directory holds a file, is checked: rule missing_twin reports
 * each api/include/<name>/messaging/<x>_protocol.hpp of its project with no
 * <x>_protocol.ts there. Rule opt_out: a model under a modeling/ directory
 * holds the :ores.ts.protocol.enabled: key. Rule unreadable_inputs: the
 * registry is not a literal.
 */
extern const compass::codec::types::descriptor protocol_twins;

}

#endif

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
#ifndef COMPASS_CROSSCHECK_CODECS_XML_BINDINGS_HPP
#define COMPASS_CROSSCHECK_CODECS_XML_BINDINGS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's XML bindings generator (regenerate_xml_bindings.sh
 * running generate_xml_bindings.py): the C++ records and codec tables of
 * each ORE document family, from ORE's schemas under external/ore/xsd.
 *
 * The families, which the script states as one command line each, are a
 * table of the codec. Each family is a header under
 * projects/ores.ore/core/include/ores.ore.core/xml/generated and a source
 * under projects/ores.ore/core/src/xml/generated, formatted by clang-format
 * as the formatter of the codec, as the script runs it. A family whose
 * schemas cannot be read, or that the generator refuses, writes nothing;
 * the others are still written.
 */
extern const compass::codec::types::descriptor xml_bindings;

}

#endif

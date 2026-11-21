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
 * @brief The C++ records and codec tables of each XML document family a
 * project lists, from the project's schemas; first ORE Studio's
 * regenerate_xml_bindings.sh and generate_xml_bindings.py.
 *
 * The project's check document compass/checks/xml_bindings.org states
 * everything: the Families table (stem, root element, root type, schemas),
 * the Exact decimals table (family, element; "*" for every real), the
 * Schema errata table (written, read) and the Settings table (setting,
 * value), which names the schema directory, the header and source
 * directories, the include and namespace prefixes, the engine and decimal
 * headers, the decimal type and the export macro, and the Licence table
 * (line), the licence comment that heads each file, a row per line. A
 * family of stem s is
 * the header s.hpp and the source s.cpp under those directories, with the
 * accessor s_document, the namespace <namespace_prefix>::s and the
 * include <include_prefix>/s.hpp, formatted by clang-format as the
 * formatter of the codec. A missing document, table, column or setting is
 * an unreadable finding and writes nothing. A family whose schemas cannot
 * be read, or that the generator refuses, writes nothing; the others are
 * still written.
 */
extern const compass::codec::types::descriptor xml_bindings;

}

#endif

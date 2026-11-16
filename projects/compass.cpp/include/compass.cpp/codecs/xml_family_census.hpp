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
#ifndef COMPASS_CPP_CODECS_XML_FAMILY_CENSUS_HPP
#define COMPASS_CPP_CODECS_XML_FAMILY_CENSUS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's xml-family-census gate: every ORE document family is
 * classified and the tree agrees.
 *
 * Reads the root element of every .xml under the corpus, comments and
 * processing instructions removed, the families the generated tables under
 * the generated directory bind with document_type doc{...}, and every
 * xml_*_tests.cpp under a tests directory of projects/. Rule
 * unclassified_family: a root the families option does not name. Rule
 * family_not_in_corpus: a family no document has as its root. Rule
 * state_disagrees: a supported family no table binds, or a migrating or
 * unused one a table binds. Rule suite_names_unused: a suite names an
 * unused family. Rule bad_family: an entry of the option is not
 * name=state. The census was the FAMILIES table of the script; it is the
 * families option, as it is the classification, not a list of excused
 * findings.
 */
extern const compass::codec::types::descriptor xml_family_census;

}

#endif

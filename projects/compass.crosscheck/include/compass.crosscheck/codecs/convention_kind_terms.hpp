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
#ifndef COMPASS_CROSSCHECK_CODECS_CONVENTION_KIND_TERMS_HPP
#define COMPASS_CROSSCHECK_CODECS_CONVENTION_KIND_TERMS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's convention-kind-terms generator
 * (generate_convention_kind_terms.py): the convention kind terms seed, the
 * trigger that validates a term's value and the trigger's drop, from the
 * ores.ore conventions mapper and convention_terms.json.
 *
 * The script's exemption tables (NOT_A_COLUMN, FILE_KIND,
 * SPELLED_OTHERWISE, READ_ELSEWHERE, ELEMENT_OF, MEMBER_ENTITY, the asset
 * classes and the schema name) are read from the script itself, so an
 * edit to one is seen. Where the script reports a problem and writes
 * nothing, the codec writes nothing.
 */
extern const compass::codec::types::descriptor convention_kind_terms;

}

#endif

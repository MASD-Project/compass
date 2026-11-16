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
#ifndef COMPASS_CPP_CODECS_TRANSLATION_UNIT_BUDGET_HPP
#define COMPASS_CPP_CODECS_TRANSLATION_UNIT_BUDGET_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's translation-unit-budget gate: no unit reflects more
 * convention families than its budget.
 *
 * Reads the family value types the registry header names, then every .cpp,
 * .hpp and .inc under the scopes that includes conventions_family_pack.hpp
 * and counts the families it names as domain::<type>. Rule over_budget: a
 * unit names more than one family, or more than the ceiling the ceilings
 * option records for it. Rule below_ceiling: a recorded unit names fewer
 * families than its ceiling. Rule bad_ceiling: a ceiling is not
 * path=count. The ceilings were the KNOWN table of the script; they stay
 * with the codec as an option, because each holds a number that only comes
 * down, which an inline exception cannot hold.
 */
extern const compass::codec::types::descriptor translation_unit_budget;

}

#endif

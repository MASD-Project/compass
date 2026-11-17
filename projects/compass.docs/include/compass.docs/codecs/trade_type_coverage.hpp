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
#ifndef COMPASS_DOCS_CODECS_TRADE_TYPE_COVERAGE_HPP
#define COMPASS_DOCS_CODECS_TRADE_TYPE_COVERAGE_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's trade-type-coverage generator
 * (generate_trade_type_coverage.py --write): the generated block of
 * doc/knowledge/domain/ore_trade_type_coverage.org.
 *
 * The block counts and tabulates each trade type of ORE's mined
 * trade_types.tsv against the trade type catalogue, the recorded
 * generated trades and the open tasks of the coverage story. Nothing is
 * written where the script refuses: a source it cannot read, a document
 * without one pair of markers, a catalogue form ORE does not read, or an
 * unsupported table that disagrees with ORE's registrations.
 */
extern const compass::codec::types::descriptor trade_type_coverage;

}

#endif

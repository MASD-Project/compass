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
#ifndef COMPASS_CROSSCHECK_CODECS_SCRIPT_LIBRARY_SEED_HPP
#define COMPASS_CROSSCHECK_CODECS_SCRIPT_LIBRARY_SEED_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's script-library-seed generator
 * (generate_script_library.py): trading_script_library_populate.sql from
 * ORE's examples/Products/Input/scriptlibrary.xml.
 *
 * Each script, its variants, results, calibrations, strikes and schedule
 * coarsenings become rows, and so do its inputs: the names its code reads
 * and does not declare or assign, typed by the call that reads them, with
 * the REQUIRE rule and the comment text that names them. A library that is
 * not XML, or an element the script reads without its text, writes
 * nothing, where the script raises.
 */
extern const compass::codec::types::descriptor script_library_seed;

}

#endif

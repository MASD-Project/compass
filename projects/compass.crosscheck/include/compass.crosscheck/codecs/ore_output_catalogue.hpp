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
#ifndef COMPASS_CROSSCHECK_CODECS_ORE_OUTPUT_CATALOGUE_HPP
#define COMPASS_CROSSCHECK_CODECS_ORE_OUTPUT_CATALOGUE_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's ore-output-catalogue generator
 * (generate_output_catalogue.py): output_catalogue_data.cpp and .hpp and
 * ore_output_formats.org, every output family ORE writes as the samples it
 * ships state it, from the expected outputs under external/ore/examples,
 * the samples under projects/ores.ore/data/samples and the reporting file
 * type seed.
 *
 * The script's tables, ore_files.py's FILES and output_family_docs.py's
 * FAMILY_DOCS are read from those modules through the Python literal
 * reader. The data files go through clang-format as the formatter of the
 * codec, as the script runs it. Where the script stops, the codec writes
 * nothing.
 */
extern const compass::codec::types::descriptor ore_output_catalogue;

}

#endif

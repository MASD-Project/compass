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
#ifndef COMPASS_CROSSCHECK_CODECS_CONVENTIONS_COVERAGE_HPP
#define COMPASS_CROSSCHECK_CODECS_CONVENTIONS_COVERAGE_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's conventions-coverage gate: tools/ore_conventions/
 * extract.py, which resolves the one canonical convention set of the ORE
 * examples, then projects/ores.seeder/datasets/conventions/propose.py
 * --check, which compares it and the conventions mined from the engine
 * with the dataset.
 *
 * A scan: each XML file under the examples (option examples) is read once
 * for its Conventions entries, and the reduce step resolves them as
 * extract.py does (variants, spelling groups, renames, decisions.tsv) into
 * the canonical table, which it reads back as propose.py reads the file.
 * Each line propose.py reports is a finding. INDEX_FIELDS and BDC are read
 * from propose.py. Rules: duplicate_row, no_source, undefined_index,
 * missing_convention, differing_convention and extract_failed, where
 * extract.py stops on a defect of decisions.tsv.
 */
extern const compass::codec::types::descriptor conventions_coverage;

}

#endif

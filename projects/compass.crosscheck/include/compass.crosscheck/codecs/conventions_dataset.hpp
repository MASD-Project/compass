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
#ifndef COMPASS_CROSSCHECK_CODECS_CONVENTIONS_DATASET_HPP
#define COMPASS_CROSSCHECK_CODECS_CONVENTIONS_DATASET_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's conventions-dataset generator
 * (projects/ores.seeder/datasets/conventions/generate_sql.py): the
 * populate script of the refdata.conventions dataset, from conventions.tsv,
 * oresmd_index_map.tsv and methodology.txt beside the script.
 *
 * The script's tables (the normalisers, KINDS, CUT_KINDS, COMPOSITE_KINDS
 * with its parts, PRIMARY_INDEX, the index fields and kinds, WORLD_KINDS,
 * NESTED_COLUMNS, the header and the dataset's names) are read from its
 * source. Its code is ported: the decoders of the nested fields, the two
 * functions COMPOSITE_KINDS names for its parts, and the text render()
 * writes. Where the script stops with an error, the codec writes nothing.
 */
extern const compass::codec::types::descriptor conventions_dataset;

}

#endif

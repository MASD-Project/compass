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
#ifndef COMPASS_CROSSCHECK_CODECS_MODEL_SQL_TYPES_HPP
#define COMPASS_CROSSCHECK_CODECS_MODEL_SQL_TYPES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's model-sql-types gate: a column's model type is the
 * type its table is created with.
 *
 * A model with a :tablename: is matched to the _create.sql under
 * projects/ores.sql/create/ whose first CREATE TABLE creates that table.
 * Each column of the model's * Columns section, a ** heading and its first
 * :type:, is compared with the column of that CREATE TABLE's body, its
 * constraints dropped, case and spaces normalised. Rule type_mismatch: the
 * two differ. Rule too_few_compared: fewer than min_compared columns
 * compared.
 */
extern const compass::codec::types::descriptor model_sql_types;

}

#endif

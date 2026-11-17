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
#ifndef COMPASS_CROSSCHECK_CODECS_MONEY_DECIMAL_COLUMNS_HPP
#define COMPASS_CROSSCHECK_CODECS_MONEY_DECIMAL_COLUMNS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's money-decimal-columns gate: a money quantity is an
 * exact decimal, and a continuous one is not.
 *
 * Reads the columns of every org file directly under a modeling/ directory
 * at any depth of projects/: a ** heading, or a :column: key outside one,
 * with its first :type: and :cpp_type:, ended by the next column or a
 * level-one section. A name is continuous when a token is a continuous word
 * (variance, recovery, ratio, tenor...), money when its head noun, past
 * qualifiers such as value or level, is a money word (rate, amount, price,
 * strike...). Rule money_not_exact: money typed a binary float or text in
 * SQL, or numeric with a binary float in C++, or a field group's money
 * member a float or string. Rule continuous_exact: a continuous quantity
 * typed numeric, or a field group's decimal. Rule too_few_columns: fewer
 * than min_columns columns read. The options excluded_columns and
 * continuous_columns name model::column keys the script lists by hand. The
 * script's baselines are replaced by inline exceptions.
 */
extern const compass::codec::types::descriptor money_decimal_columns;

}

#endif

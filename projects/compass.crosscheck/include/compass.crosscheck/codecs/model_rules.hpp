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
#ifndef COMPASS_CROSSCHECK_CODECS_MODEL_RULES_HPP
#define COMPASS_CROSSCHECK_CODECS_MODEL_RULES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief The rules ORE Studio's gates state about its codegen models, over
 * each model as compass.model reads it: one scan of documents whose map
 * builds a model's outline once, the view the loaders read, and runs every
 * rule that needs that model alone, and whose reduce runs the rules that
 * need other files from the facts the maps kept. An edit to one model maps
 * that model alone.
 *
 * A model is a document under a modeling/ directory of projects/ whose
 * #+type: is a type ores.codegen loads.
 *
 * Rules money_not_exact and continuous_exact (money-decimal-columns): the
 * columns of an entity or junction model, the children of its Columns
 * section, and the fields of a field group, with the :type: and
 * :cpp_type: of their drawers. A name is continuous when a token is a
 * continuous word (variance, recovery, ratio, tenor...), money when its
 * head noun, past qualifiers such as value or level, is a money word
 * (rate, amount, price, strike...). Money typed a binary float or text in
 * SQL, or numeric with a binary float in C++, or a field group's money
 * field typed a float or string, is not exact; a continuous quantity
 * typed numeric, or a field group's decimal, is exact. The options
 * excluded_columns and continuous_columns name model::column keys the
 * script lists by hand.
 */
extern const compass::codec::types::descriptor model_rules;

}

#endif

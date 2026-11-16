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
#ifndef COMPASS_MODEL_IO_JSON_HPP
#define COMPASS_MODEL_IO_JSON_HPP

#include <string>
#include <string_view>
#include "compass.model/ops/builder.hpp"
#include "compass.model/types/model_tables.hpp"

namespace compass::model::io {

/**
 * @brief Appends @p value as canonical JSON, then a line end.
 *
 * The format is what Python writes with json.dumps(value, sort_keys=True,
 * indent=2, ensure_ascii=False): keys in code point order, one member per
 * line indented two spaces per level, ": " after a key, and only the
 * characters JSON requires escaped. build/scripts/codegen_model_dump.py writes ores.codegen's
 * models in the same format, so the two dumps compare as text.
 */
void write_json(std::string& out, const types::model_tables& t,
    types::value_id value);

/**
 * @brief Reads the JSON object or array @p text into new values of @p b and
 * returns the value, which nothing holds.
 *
 * The model has no real numbers, so a number with a fraction or an exponent
 * becomes a string holding its text; Python's str() of a float that
 * json.dumps wrote is the same text. Throws std::runtime_error on text that
 * is not JSON, or whose top level is not an object or an array.
 */
types::value_id read_json(ops::builder& b, std::string_view text);

}

#endif

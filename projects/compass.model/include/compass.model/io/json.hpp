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

}

#endif

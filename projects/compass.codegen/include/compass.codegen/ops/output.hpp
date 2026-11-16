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
#ifndef COMPASS_CODEGEN_OPS_OUTPUT_HPP
#define COMPASS_CODEGEN_OPS_OUTPUT_HPP

#include <string>
#include <string_view>
#include "compass.codegen/types/compiled_template.hpp"
#include "compass.model/types/model_tables.hpp"

namespace compass::codegen::ops {

/**
 * @brief Appends the text ores.codegen writes for the template
 * @p template_name, compiled as @p t, from the context object @p context,
 * before any formatter runs.
 *
 * That is core.render_template followed by _substitute_paste_markers:
 *
 * - The licence the output's language carries gets the generated-file
 *   marker naming the template after it: cpp_license for a .hpp, .cpp or
 *   .inc template, ts_license for a .ts one and cmake_license, in #
 *   comments, for a cmake_ one.
 * - In the four templates that declare C++ members, a field's
 *   cpp_value_init is "{}" for a scalar cpp_type and "" otherwise.
 * - The template renders without HTML escaping.
 * - An org output gets the #+generated: keyword after its first :END: line,
 *   or at its top when it has none.
 * - Each <<paste:UUID>> line becomes the blocks of the model's
 *   implementations of that kind, or goes when there are none.
 */
void render_output(std::string& out, std::string_view template_name,
    const types::compiled_template& t,
    const compass::model::types::model_tables& values,
    compass::model::types::value_id context);

}

#endif

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
#ifndef COMPASS_CODEGEN_OPS_COMPILE_HPP
#define COMPASS_CODEGEN_OPS_COMPILE_HPP

#include <stdexcept>
#include <string>
#include "compass.codegen/types/compiled_template.hpp"

namespace compass::codegen::ops {

/**
 * @brief A template pystache refuses to parse: a section closed with
 * another name or not opened, or a delimiter change without two delimiters.
 */
class template_error final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Parses @p source once into the nodes rendering walks.
 *
 * The parse is pystache 0.6's: a tag is found as its regular expression
 * finds it, and a tag other than an interpolation that is alone on its line,
 * after spaces and tabs only, takes that whitespace and the line end with it.
 * Unlike the Mustache spec, whitespace after such a tag keeps it from being
 * alone. An unclosed section is an error, where pystache would render a
 * fragment of the template.
 */
[[nodiscard]] types::compiled_template compile(std::string source);

}

#endif

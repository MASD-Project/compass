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
#ifndef COMPASS_CROSSCHECK_OPS_PYTHON_NUMBERS_HPP
#define COMPASS_CROSSCHECK_OPS_PYTHON_NUMBERS_HPP

#include <optional>
#include <string>
#include <string_view>

/**
 * @brief How a generator script turns a text into a number and back:
 * str(int(text)) and repr(float(text)).
 */
namespace compass::crosscheck::ops {

/**
 * @brief str(int(@p text)): the text without its surrounding spaces, its
 * sign kept only when negative, its underscores and leading zeros gone;
 * nothing where int() raises ValueError.
 */
[[nodiscard]] std::optional<std::string> python_int(std::string_view text);

/**
 * @brief repr(@p value): the shortest digits that read back as the value,
 * in fixed notation from 1e-4 up to 1e16 with at least one digit after the
 * point, and otherwise as d.ddde+XX.
 */
[[nodiscard]] std::string python_float_repr(double value);

/**
 * @brief repr(float(@p text)); nothing where float() raises ValueError.
 */
[[nodiscard]] std::optional<std::string> python_float(std::string_view text);

}

#endif

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
#ifndef COMPASS_CROSSCHECK_OPS_WORDS_HPP
#define COMPASS_CROSSCHECK_OPS_WORDS_HPP

#include <string>
#include <string_view>
#include <vector>

namespace compass::crosscheck::ops {

/**
 * @brief @p text with its ASCII letters lowered, as str.lower() lowers the
 * names the scripts read.
 */
[[nodiscard]] std::string lowered(std::string_view text);

/**
 * @brief The pieces of @p text between each @p separator, as
 * str.split(separator) gives them, empty pieces kept.
 */
[[nodiscard]] std::vector<std::string_view> split(std::string_view text,
    char separator);

/**
 * @brief @p text with each run of spaces turned into one blank, as
 * re.sub(r"\s+", " ", text) leaves it.
 */
[[nodiscard]] std::string collapse_spaces(std::string_view text);

}

#endif

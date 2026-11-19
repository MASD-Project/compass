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
#ifndef COMPASS_CPP_CODECS_FORMAT_CHECK_HPP
#define COMPASS_CPP_CODECS_FORMAT_CHECK_HPP

#include <string_view>
#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief The one option of a format check: its definition, in the text
 * ops::parse_format_definition reads.
 */
inline constexpr std::string_view format_definition_key = "definition";

/**
 * @brief The function of every format check, a formatting check: it emits
 * each file the definition covers, at its path, as the checkout holds it.
 * A definition that does not parse covers no file.
 */
[[nodiscard]] compass::codec::types::corpus_function<
    compass::codec::types::file_output>
format_check_function() noexcept;

/**
 * @brief The formatter of a format check, from its definition option, as
 * ops::formatter_of gives it.
 * @throws std::runtime_error as ops::formatter_of does, and when the
 * definition does not parse.
 */
[[nodiscard]] compass::codec::types::formatters_function
format_check_formatters() noexcept;

}

#endif

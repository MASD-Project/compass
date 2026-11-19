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
#ifndef COMPASS_CPP_CODECS_LIMIT_HPP
#define COMPASS_CPP_CODECS_LIMIT_HPP

#include <string_view>
#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief The one option of a limit check: its definition, in the text
 * ops::parse_limit_definition reads.
 */
inline constexpr std::string_view limit_definition_key = "definition";

/**
 * @brief The function of every limit check. It maps each file the
 * definition covers to ops::limit_fact, and reduces with
 * ops::limit_findings. A definition that does not parse covers no file.
 */
[[nodiscard]] compass::codec::types::scan_function limit_function() noexcept;

}

#endif

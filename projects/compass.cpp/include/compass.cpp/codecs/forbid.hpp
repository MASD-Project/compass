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
#ifndef COMPASS_CPP_CODECS_FORBID_HPP
#define COMPASS_CPP_CODECS_FORBID_HPP

#include <string_view>
#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief The one option of a forbid check: its definition, in the text
 * ops::parse_forbid_definition reads.
 */
inline constexpr std::string_view forbid_definition_key = "definition";

/**
 * @brief The function of every forbid check. A check is a descriptor of
 * its own, named for it, with one option, forbid_definition_key, whose
 * default is its definition, and the rules of the definition, in order,
 * then a rule for its examples. It maps each file the definition covers
 * through ops::forbid_scan, and reduces by checking the definition's
 * examples with ops::check_examples. A definition that does not parse
 * covers no file and has no examples.
 */
[[nodiscard]] compass::codec::types::scan_function forbid_function() noexcept;

}

#endif

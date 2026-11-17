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
#ifndef COMPASS_CPP_CODECS_CATALOGUE_HPP
#define COMPASS_CPP_CODECS_CATALOGUE_HPP

#include <span>
#include <string_view>
#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief The codecs this component ships, in name order, each with the
 * code hash of this component. Each is off by
 * default.
 */
[[nodiscard]] std::span<const compass::codec::types::descriptor>
catalogue() noexcept;

/**
 * @brief The hash of the code the codecs of this component are computed
 * by: the sources of this component and of every component and library it
 * links, and the compiler. The build writes it.
 */
[[nodiscard]] std::string_view code_hash() noexcept;

}

#endif

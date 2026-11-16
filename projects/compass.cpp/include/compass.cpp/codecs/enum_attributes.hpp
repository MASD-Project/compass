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
#ifndef COMPASS_CPP_CODECS_ENUM_ATTRIBUTES_HPP
#define COMPASS_CPP_CODECS_ENUM_ATTRIBUTES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's enum-attributes gate: no visibility macro decorates
 * an enum.
 *
 * Reads every .hpp, .h, .cpp, .ipp and .cc file under projects/ with its
 * comments and literals erased. A visibility macro is BOOST_SYMBOL_EXPORT,
 * BOOST_SYMBOL_IMPORT or any macro one of those files defines as a symbol
 * visibility or DLL storage attribute. Rule visibility_macro_on_enum: an
 * enum declaration names such a macro before its name; clang-cl rejects
 * it under -Werror, and nothing else reports it.
 */
extern const compass::codec::types::descriptor enum_attributes;

}

#endif

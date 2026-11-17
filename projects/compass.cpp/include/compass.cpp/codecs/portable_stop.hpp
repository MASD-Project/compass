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
#ifndef COMPASS_CPP_CODECS_PORTABLE_STOP_HPP
#define COMPASS_CPP_CODECS_PORTABLE_STOP_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's portable-stop gate: no code outside ores.platform
 * names a C++20 stop type.
 *
 * Reads every C++ source and fragment under projects/, outside
 * ores.platform and the node_modules, venv, .venv, build and external
 * directories, through the lexer: comments, literals and code an #if 0
 * excludes aside. Rule stop_type_outside_platform: a line names
 * std::jthread, std::stop_token, std::stop_source or std::stop_callback,
 * spaces around :: allowed, or includes <stop_token>; Apple's libc++ has
 * none of them.
 */
extern const compass::codec::types::descriptor portable_stop;

}

#endif

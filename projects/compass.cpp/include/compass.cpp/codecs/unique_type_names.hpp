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
#ifndef COMPASS_CPP_CODECS_UNIQUE_TYPE_NAMES_HPP
#define COMPASS_CPP_CODECS_UNIQUE_TYPE_NAMES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's unique-type-names gate: no two headers define a type
 * of the same name in one namespace.
 *
 * Reads every .hpp under an include or tests directory of projects/ores.*,
 * outside build, with its comments, literals and preprocessor lines
 * blanked, and collects the qualified name of each class or struct defined
 * directly in named namespaces. A header under include can meet any other;
 * a test header meets the headers of its own tests directory. Rule
 * type_defined_twice: two headers that can meet in one binary define the
 * same qualified name, reported on the first of them.
 */
extern const compass::codec::types::descriptor unique_type_names;

}

#endif

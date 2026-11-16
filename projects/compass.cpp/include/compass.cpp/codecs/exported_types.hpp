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
#ifndef COMPASS_CPP_CODECS_EXPORTED_TYPES_HPP
#define COMPASS_CPP_CODECS_EXPORTED_TYPES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's exported-types gate: no library exports a type that
 * none of its translation units compiles.
 *
 * A library is the directory two levels above an
 * projects/<component>/<part>/include/<name>/export.hpp that defines an
 * ORES_..._EXPORT macro as BOOST_SYMBOL... behind an ORES_..._LIBRARY
 * guard. Rule uncompiled_exported_type: a header under the library's
 * include/ declares "class MACRO name" or "struct MACRO name", and no
 * .cpp under its src/ reaches the header through #include "..." lines,
 * each resolved against the including file's directory and then every
 * projects/<component>/<part>/include. On Windows such a type is imported
 * by every consumer and defined by nobody. Reads every file of the
 * checkout it opens and the listing of all paths.
 */
extern const compass::codec::types::descriptor exported_types;

}

#endif

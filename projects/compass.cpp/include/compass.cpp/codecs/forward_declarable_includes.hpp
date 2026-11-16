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
#ifndef COMPASS_CPP_CODECS_FORWARD_DECLARABLE_INCLUDES_HPP
#define COMPASS_CPP_CODECS_FORWARD_DECLARABLE_INCLUDES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's forward-declarable-includes gate: a header that only
 * names a type includes its declaration, not its definition.
 *
 * Reads the types of the list option
 * (build/scripts/forward_declarable_types.list), then every .hpp, .h and
 * .ipp under projects/ outside venv, .venv, node_modules, build and _deps.
 * A header that includes a listed type's defining header, other than that
 * header itself, is scanned with its comments and include lines gone: a
 * use names the type when it follows class, struct or enum class, is
 * followed by * or &, is in a using or typedef, or is an argument of
 * shared_ptr or weak_ptr; any other use, or a call through a reference,
 * pointer or smart pointer that holds it, needs the definition. Rule
 * includes_definition_it_only_names: every use only names the type,
 * reported on the include line. Rule bad_type_list: the list is malformed
 * or empty. The script's baseline is not read: an accepted offender
 * carries an inline exception on its include line.
 */
extern const compass::codec::types::descriptor forward_declarable_includes;

}

#endif

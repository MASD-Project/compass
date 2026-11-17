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
#ifndef COMPASS_DOCS_CODECS_COMPONENT_DOCS_HPP
#define COMPASS_DOCS_CODECS_COMPONENT_DOCS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's component-docs gate (validate_docs.py): every
 * component has a complete overview and a diagram.
 *
 * A component is a directory projects/ores.*, a part a directory under it
 * other than modeling/. A component none of whose parts has an overview is
 * judged on its own modeling/; each part is judged on its modeling/, or,
 * with none, only when it has include/ and src/. An overview needs an
 * :ID:, #+type: component, a #+description: and the sections Summary,
 * Inputs, Outputs, Entry points, Dependencies and See also, and its
 * modeling/ a .puml. A top-level name may not extend another's. A page
 * under doc/ may not hold a bare bracket link inside one backtick span,
 * outside src and example blocks. The exceptions option names the file of
 * CODE component lines that excuse a rule for one component.
 */
extern const compass::codec::types::descriptor component_docs;

}

#endif

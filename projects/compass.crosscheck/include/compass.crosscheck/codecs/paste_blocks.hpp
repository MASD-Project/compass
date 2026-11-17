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
#ifndef COMPASS_CROSSCHECK_CODECS_PASTE_BLOCKS_HPP
#define COMPASS_CROSSCHECK_CODECS_PASTE_BLOCKS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's paste-blocks gate: every ores:: name a model's
 * pasted C++ block names is declared somewhere.
 *
 * Reads every org file under projects/<component>/modeling/, at any depth, and each
 * cpp source block whose header holds :implements. A header is every .hpp
 * with an include directory on its path; its comments and literals are
 * blanked and each identifier is recorded with the namespace, class, struct,
 * union or enum braces it sits in. Rule unresolved_name: a name rooted at
 * ores in a block whose scope declares no such identifier. Rule
 * nothing_read: no block, no header, or no name to check.
 */
extern const compass::codec::types::descriptor paste_blocks;

}

#endif

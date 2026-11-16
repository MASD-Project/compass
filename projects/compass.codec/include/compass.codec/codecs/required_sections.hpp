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
#ifndef COMPASS_CODEC_CODECS_REQUIRED_SECTIONS_HPP
#define COMPASS_CODEC_CODECS_REQUIRED_SECTIONS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::codec::codecs {

/**
 * @brief Reports each section a document lacks that its registered type
 * requires.
 *
 * A document has a section when one of its headings, at any level, has
 * that title. A section the contract calls optional is not in the type
 * registry, so its absence is never reported. The range of a finding is
 * the value of the #+type: keyword, which is what makes the section
 * required, and the fix names the heading to add.
 */
extern const types::descriptor required_sections;

}

#endif

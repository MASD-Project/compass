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
#ifndef COMPASS_CODEC_CODECS_EVIDENCE_LOCATION_HPP
#define COMPASS_CODEC_CODECS_EVIDENCE_LOCATION_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::codec::codecs {

/**
 * @brief Reports each evidence document that is not under an evidence/
 * directory.
 *
 * The evidence tag takes a document out of every contract and out of the
 * input of every codec that does not ask for evidence, so its path must
 * say so too: a reader who finds a page under evidence/ knows it may be a
 * record, and one who finds it anywhere else takes it for a live page.
 * The range of a finding is the value of the #+filetags: keyword that
 * holds the tag.
 */
extern const types::descriptor evidence_location;

}

#endif

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
#ifndef COMPASS_CODEC_CODECS_TANGLE_HPP
#define COMPASS_CODEC_CODECS_TANGLE_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::codec::codecs {

/**
 * @brief Emits the files the source blocks of each document tangle to,
 * byte for byte as org-babel-tangle-file writes them under emacs -Q.
 *
 * See ops::tangle for the rules. A target with a block the codec cannot
 * tangle is not emitted; tangle_check reports why. Two documents that
 * tangle to one path are refused by ops::check_outputs.
 */
extern const types::descriptor tangle;

}

#endif

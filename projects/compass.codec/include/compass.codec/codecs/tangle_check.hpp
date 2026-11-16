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
#ifndef COMPASS_CODEC_CODECS_TANGLE_CHECK_HPP
#define COMPASS_CODEC_CODECS_TANGLE_CHECK_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::codec::codecs {

/**
 * @brief Reports each source block the tangle codec leaves out, and so the
 * target it does not write.
 *
 * Rule unsupported_header_argument: the block sets a header argument, a
 * switch or a :tangle value whose effect on the bytes tangling does not
 * reproduce, or the document sets header-args. Rule
 * target_outside_checkout: the target is absolute or leaves the checkout.
 * A codec of its own because a codec emits files or diagnostics, not both;
 * enable it with tangle.
 */
extern const types::descriptor tangle_check;

}

#endif

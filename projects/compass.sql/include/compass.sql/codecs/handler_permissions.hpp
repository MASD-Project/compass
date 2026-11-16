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
#ifndef COMPASS_SQL_CODECS_HANDLER_PERMISSIONS_HPP
#define COMPASS_SQL_CODECS_HANDLER_PERMISSIONS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::sql::codecs {

/**
 * @brief ORE Studio's handler-permissions gate: every handler permission
 * code is seeded.
 *
 * Reads the codes the seeds script passes as literals, then every
 * *_handler.hpp and *_handler.cpp in a messaging directory of each
 * component the components option names. Rule unseeded_permission: a
 * handler calls has_permission with a code no seed defines; a code built
 * as a literal followed by + is a prefix that any seed starting with it
 * satisfies.
 */
extern const compass::codec::types::descriptor handler_permissions;

}

#endif

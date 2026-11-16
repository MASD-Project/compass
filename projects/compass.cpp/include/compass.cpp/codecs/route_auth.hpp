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
#ifndef COMPASS_CPP_CODECS_ROUTE_AUTH_HPP
#define COMPASS_CPP_CODECS_ROUTE_AUTH_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's route-auth gate: every HTTP route declares its
 * authorisation.
 *
 * Reads every .cpp and .hpp under projects/ores.http outside tests and
 * build, with comments and literals masked. An add_route call's argument
 * is its builder chain, or the chain an auto local of that name is bound
 * to, followed through locals bound to another's build(). Rule
 * route_without_auth: the chain calls none of auth_required, auth_optional
 * or roles. Rule too_few_routes: fewer than 20 add_route calls, so the
 * scan has gone blind.
 */
extern const compass::codec::types::descriptor route_auth;

}

#endif

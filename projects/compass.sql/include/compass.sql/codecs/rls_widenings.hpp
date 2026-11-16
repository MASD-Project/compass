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
#ifndef COMPASS_SQL_CODECS_RLS_WIDENINGS_HPP
#define COMPASS_SQL_CODECS_RLS_WIDENINGS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::sql::codecs {

/**
 * @brief ORE Studio's rls-widenings gate: no row-level security policy
 * widens the system tenant.
 *
 * Reads every .sql under projects/ores.sql/create with its -- comments
 * blanked. Rule system_tenant_widening: a create policy whose body
 * compares ores_iam_current_tenant_id_fn() with
 * ores_utility_system_tenant_id_fn(), so a system-tenant session sees
 * every tenant. The script's table of excused policies is not read: each
 * becomes an inline exception on its create policy line, which is reported
 * stale once the policy no longer widens, as the script reports a stale
 * entry.
 */
extern const compass::codec::types::descriptor rls_widenings;

}

#endif

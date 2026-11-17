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
#ifndef COMPASS_SQL_CODECS_RLS_COVERAGE_HPP
#define COMPASS_SQL_CODECS_RLS_COVERAGE_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::sql::codecs {

/**
 * @brief The row-level security checks of ORE Studio's er-diagram gate:
 * RLS_001 and RLS_002 of plantuml_er_parse_sql.py's
 * validate_rls_policies, which the gate runs with --strict.
 *
 * A scan of every *_create.sql: the fact of a script is the tables it
 * creates with their columns, the tables it enables row-level security
 * on and those a restrictive policy naming party_id is on, all read
 * through ops::model_of, so comments and literals state none of them.
 * Every table that has a tenant_id column needs an ALTER TABLE ... ENABLE
 * ROW LEVEL SECURITY in some *_create.sql (rule missing_rls, RLS_001),
 * and one that also has a party_id column needs a CREATE POLICY ... AS
 * RESTRICTIVE that names party_id (rule missing_party_isolation,
 * RLS_002). The ignore file (option ignore_file) is read as the script reads it:
 * each line "<code pattern> <table pattern>", with fnmatch's *, ? and
 * [...], excuses the findings of that code on those tables. Each finding
 * is on the line that creates the table, in the script that last defines
 * it. RLS_003, which asks that every *_rls_policies_create.sql be
 * included from rls/rls_create.sql, is not ported.
 */
extern const compass::codec::types::descriptor rls_coverage;

}

#endif

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
#ifndef COMPASS_SQL_CODECS_SQL_REACHABILITY_HPP
#define COMPASS_SQL_CODECS_SQL_REACHABILITY_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::sql::codecs {

/**
 * @brief ORE Studio's sql-reachability gate: every SQL file is reached by
 * a schema entry point.
 *
 * Walks the psql includes, \i from projects/ores.sql and \ir from the
 * including file, starting at the .sql files directly under
 * projects/ores.sql and the four entry points nothing includes:
 * drop/drop.sql, drop/drop_database.sql, drop/drop_test_databases.sql and
 * populate/ip2country/ip2country_import.sql. Rule unreachable_file: a .sql
 * under create, populate or drop the walk does not reach. Rule
 * missing_include: an include or entry point names no file, which aborts
 * psql.
 */
extern const compass::codec::types::descriptor sql_reachability;

}

#endif

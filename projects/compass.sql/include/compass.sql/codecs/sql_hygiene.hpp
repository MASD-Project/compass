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
#ifndef COMPASS_SQL_CODECS_SQL_HYGIENE_HPP
#define COMPASS_SQL_CODECS_SQL_HYGIENE_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::sql::codecs {

/**
 * @brief ORE Studio's sql-hygiene gate over every .sql file under the
 * directories of the option roots, comma-separated, default
 * projects/ores.sql.
 *
 * Rule unterminated_block_comment: a block comment is still open at the
 * end of the file, PostgreSQL nesting them. Rule duplicate_index: one
 * "create index if not exists" appears twice for the same name, table and
 * columns. Rule comment_splits_literals: a comment stands between two
 * string literals, which PostgreSQL joins only across whitespace. Rule
 * not_re_runnable: DDL a second run of the file fails on.
 */
extern const compass::codec::types::descriptor sql_hygiene;

}

#endif

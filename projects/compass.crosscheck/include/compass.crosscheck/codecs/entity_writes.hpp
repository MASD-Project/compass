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
#ifndef COMPASS_CROSSCHECK_CODECS_ENTITY_WRITES_HPP
#define COMPASS_CROSSCHECK_CODECS_ENTITY_WRITES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's entity-writes gate: only the repositories write an
 * entity table.
 *
 * Rule sql_writer: a function of a .sql file under projects/ores.sql/create/
 * whose body, as compass.sql's ops::model_of reads it, writes an
 * ores_..._tbl table with INSERT INTO, UPDATE or DELETE FROM, in its code
 * or in the SQL of a string literal it executes, unless its name starts
 * with ores_iam_purge_. Rule cpp_writer: a .cpp or
 * .hpp under a projects/ores.* component, outside ores.testing and any tests
 * or build directory, that writes such a table and is not named
 * <name>_repository for a table ores_<component>_<name>[s]_tbl of each one
 * it writes. The script's baseline is replaced by inline exceptions: a
 * function's on its line, a file's with compass:except-file.
 */
extern const compass::codec::types::descriptor entity_writes;

}

#endif

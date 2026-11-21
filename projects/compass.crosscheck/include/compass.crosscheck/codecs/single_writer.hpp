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
#ifndef COMPASS_CROSSCHECK_CODECS_SINGLE_WRITER_HPP
#define COMPASS_CROSSCHECK_CODECS_SINGLE_WRITER_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's single-writer gate: one table in one tenant has one
 * writer, a populate script or a DQ publish function, not both.
 *
 * Rule second_writer: a .sql file under projects/ores.sql/populate/ that
 * inserts into a table some *_publish_from_dq_create.sql file under
 * projects/ores.sql/create/ also inserts into, reported once per table on
 * the line of the file's first insert into it. An insert is INSERT INTO
 * and the table name, in any case and optionally quoted, outside SQL
 * comments, which are removed block comments first, as the script did. A
 * populate script that writes another tenant's rows, or rows disjoint from
 * the publish function's, is not a second copy and says so in a
 * compass:except comment, which replaces the script's list of exceptions;
 * an exception no finding needs is reported as stale.
 */
extern const compass::codec::types::descriptor single_writer;

}

#endif

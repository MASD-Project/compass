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
#ifndef COMPASS_ENV_CODECS_ENV_FORMAT_HPP
#define COMPASS_ENV_CODECS_ENV_FORMAT_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::env::codecs {

/**
 * @brief Whether a project's environment file format is versioned whole:
 * every variable its check document declares names the version that added
 * it, and that version says so.
 *
 * The check document whose Definition names this codec gives the format
 * as the tables compass.env/ops/format.hpp reads; compass env generate
 * writes the file from the same tables. The file itself is uncommitted, so
 * no committed text can show a variable written without a row: the table
 * is the only place a variable comes from, and this codec checks the
 * table.
 *
 * The rules: a variable that names no version that exists
 * (unknown_version); a variable added or removed in a version after the
 * baseline that does not list it (undeclared), which is what a new row
 * with an old version is; a version that lists a variable no row is added
 * or removed in (unknown_variable); a version after the baseline that adds
 * and removes nothing and is not format only (empty_version); and a
 * document, table, column, setting, kind or template that does not read
 * (unreadable), which checks nothing else.
 */
extern const compass::codec::types::descriptor env_format;

}

#endif

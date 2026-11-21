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
 * and removes nothing and is not format only (empty_version); a document,
 * table, column, setting, kind or template that does not read
 * (unreadable), which checks nothing else; and two live rows of one name,
 * one of them without a when, that some service could both be given
 * (duplicate_variable), since such a file would hold the variable twice.
 *
 * A row's when is a condition: a template, which holds when it expands to
 * something; or <template> = <template>, which holds when the two expand
 * to the same text, and <template> != <template>, when they do not. The
 * operator has a blank on each side, the first one outside braces splits
 * the condition, and the blanks around each side are dropped:
 * {FLAG} != 0, {FLAG} = 0, {A} = {B}. A row whose condition fails is not
 * written, and its name keeps the value another row of it wrote, or is
 * empty for later templates. So two rows of one name with exclusive
 * conditions write one line, in the place of the row whose condition
 * holds. The names a condition uses are checked as any template's are: an
 * input, an earlier row, or the row's own variable.
 */
extern const compass::codec::types::descriptor env_format;

}

#endif

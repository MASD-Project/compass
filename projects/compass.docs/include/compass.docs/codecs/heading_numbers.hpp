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
#ifndef COMPASS_DOCS_CODECS_HEADING_NUMBERS_HPP
#define COMPASS_DOCS_CODECS_HEADING_NUMBERS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's heading-numbers gate: no org heading carries its
 * own number.
 *
 * Reads every document under doc/ or projects/ with no build, external,
 * venv, node_modules, .git or .cache directory on its path, line by line as
 * str.splitlines(keepends=True) splits it. A line that opens a src,
 * example, quote, export or comment block as its first token opens one,
 * and each end tag anywhere on a line closes one. Rule numbered_heading:
 * outside a block, a heading whose title starts with a number such as 2.1.
 * or 3). Rule unreadable_document: a document that is not UTF-8, which the
 * script fails on.
 */
extern const compass::codec::types::descriptor heading_numbers;

}

#endif

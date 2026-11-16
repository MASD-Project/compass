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
#ifndef COMPASS_ORG_IO_DUMP_HPP
#define COMPASS_ORG_IO_DUMP_HPP

#include <ostream>
#include <string_view>
#include "compass.org/types/tables.hpp"

namespace compass::org::io {

/**
 * @brief Writes one document as text, one line per element and object in
 * document order.
 *
 * A line is the org-element type, then :begin, :end, :contents-begin,
 * :contents-end and :post-blank, then the fields the detail tables hold as
 * key=value pairs:
 *
 *   headline 154 430 190 430 0 level=1 todo=OPEN ... title=Heading one
 *
 * Positions are byte offsets into @p source. An empty contents range prints
 * as "- -". Each level of nesting indents by two spaces. The objects of an
 * element print under it, before its child elements. An affiliated keyword
 * prints under its owner as "affiliated key=<name> value=<text>", sorted by
 * name; a caption prints no value and is followed by its objects. Where a
 * keyword that org-element keeps once is written twice, the last one
 * prints. In a value, a backslash, a line feed, a carriage return and a tab
 * print as two characters: a backslash, then a backslash, n, r or t. The
 * path of a link prints as org-element reads it: on one line when it wraps,
 * and without the backslashes that escape brackets.
 *
 * build/scripts/org_element_dump.el prints org-element's parse tree in the
 * same format, so the two can be compared line by line.
 *
 * @p tables must hold one document, as the parser fills it.
 */
void dump(std::ostream& os, std::string_view source,
    const types::tables& tables);

}

#endif

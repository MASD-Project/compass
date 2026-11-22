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
#ifndef COMPASS_HTML_OPS_ANCHORS_HPP
#define COMPASS_HTML_OPS_ANCHORS_HPP

#include <string>
#include <string_view>
#include <vector>
#include "compass.org/types/tables.hpp"

namespace compass::html::ops {

/**
 * @brief The anchor of each heading of a document, by heading row.
 *
 * A heading's anchor is its :ID: property. A heading with no :ID:, or with
 * one an earlier heading already took, has the slug of its heading path:
 * the titles from the top heading down, as plain text, lower case, every run
 * of characters other than ASCII letters, digits and bytes of multi-byte
 * characters made one hyphen. A slug that an earlier heading took gets -2,
 * -3 and so on, so every anchor of a document is unique.
 */
[[nodiscard]] std::vector<std::string> heading_anchors(
    std::string_view source, const org::types::tables_view& t);

}

#endif

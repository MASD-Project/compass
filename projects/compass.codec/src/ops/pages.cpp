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
#include "compass.codec/ops/pages.hpp"

namespace compass::codec::ops {

std::expected<types::document, std::string> page_of(
    const types::corpus_view& v, std::string_view key) {
    const auto id = v.option(key);
    if (id.empty())
        return std::unexpected("the option '" + std::string(key) +
            "' names no page; set it to the :ID: of the page");
    if (auto found = v.find_id(id))
        return *found;
    return std::unexpected("no document has the :ID: " + std::string(id) +
        " that the option '" + std::string(key) + "' names");
}

}

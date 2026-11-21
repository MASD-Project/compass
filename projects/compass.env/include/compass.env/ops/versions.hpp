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
#ifndef COMPASS_ENV_OPS_VERSIONS_HPP
#define COMPASS_ENV_OPS_VERSIONS_HPP

#include <expected>
#include <string>
#include <string_view>
#include <vector>
#include "compass.env/types/env_format.hpp"

namespace compass::env::ops {

/**
 * @brief A version to record.
 */
struct version_request {
    std::string description;
    std::vector<std::string> adds;
    std::vector<std::string> removes;
    bool format_only = false;
    std::string date;
};

/**
 * @brief @p document, the check document @p f was read from, with a row
 * for the version after f's current one appended to its Versions table,
 * and that version set as the added version of each row of its Variables
 * table that @p r adds and the removed version of each live row it
 * removes.
 *
 * A row it touches keeps the table's alignment when its cells fit, and
 * the table is aligned again when they do not.
 *
 * @return the new text, or why the version cannot be recorded: it adds and
 * removes nothing and is not format only, it has no description, or it
 * names a variable no row has.
 */
[[nodiscard]] std::expected<std::string, std::string> append_version(
    std::string_view document, const types::env_format& f,
    const version_request& r);

}

#endif

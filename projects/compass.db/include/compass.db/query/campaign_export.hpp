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
#ifndef COMPASS_DB_QUERY_CAMPAIGN_EXPORT_HPP
#define COMPASS_DB_QUERY_CAMPAIGN_EXPORT_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "compass.db/io/connection.hpp"
#include "compass.db/types/campaign_export.hpp"

namespace compass::db::query {

/**
 * @brief The rows of every campaign of @p environment, or only that of the
 * story whose :ID: is @p story, in id order, read in one snapshot. A
 * campaign whose story the environment no longer holds has an empty
 * story_path.
 * @throws unknown_environment when the environment does not exist.
 * @throws io::error when a statement fails.
 */
[[nodiscard]] std::vector<types::campaign_rows> read_campaigns(
    io::connection& c, const std::string& environment,
    const std::optional<std::string>& story);

/**
 * @brief Sets exported_at of each campaign of @p ids to now.
 * @throws io::error when the statement fails.
 */
void mark_exported(io::connection& c, std::span<const std::int64_t> ids);

}

#endif

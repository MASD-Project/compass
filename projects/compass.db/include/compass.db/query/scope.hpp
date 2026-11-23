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
#ifndef COMPASS_DB_QUERY_SCOPE_HPP
#define COMPASS_DB_QUERY_SCOPE_HPP

#include <string>
#include <vector>
#include "compass.db/io/connection.hpp"
#include "compass.db/types/search.hpp"

namespace compass::db::query {

/**
 * @brief SQL text and the parameters it binds, numbered as they are added.
 */
struct statement {
    std::string sql;
    std::vector<io::parameter> parameters;

    std::string bind(std::string value) {
        parameters.emplace_back(std::move(value));
        return "$" + std::to_string(parameters.size());
    }
};

/**
 * @brief An environment as a query scopes by it: its id, when it was last
 * synced, as ISO 8601 in UTC or empty, and the database's text search
 * configuration.
 */
struct environment_row {
    std::string id;
    std::string synced_at;
    std::string text_search;
};

/**
 * @brief The environment named @p name.
 * @throws unknown_environment when the database holds none of that name.
 * @throws io::error when a statement fails or the database has no text
 * search configuration.
 */
[[nodiscard]] environment_row find_environment(io::connection& c,
    const std::string& name);

/**
 * @brief Appends the WHERE clause of the placements p of the environment
 * @p environment_id and their documents d that @p v and @p f let through.
 */
void where_documents(statement& s, const std::string& environment_id,
    const types::view_filter& v, const types::document_filter& f);

}

#endif

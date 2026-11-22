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
#ifndef COMPASS_DB_QUERY_SEARCH_HPP
#define COMPASS_DB_QUERY_SEARCH_HPP

#include <stdexcept>
#include <string>
#include "compass.db/io/connection.hpp"
#include "compass.db/types/search.hpp"

namespace compass::db::query {

/**
 * @brief The request named an environment the database does not hold.
 */
class unknown_environment : public std::runtime_error {
public:
    explicit unknown_environment(const std::string& name)
        : std::runtime_error("no environment named " + name) {}
};

/**
 * @brief The sections of @p r's environment that match its query, inside
 * @p v and the request's filters, ordered by @p how and paged by the
 * request's limit and offset. An empty query matches nothing.
 * @throws unknown_environment when the environment does not exist.
 * @throws io::error when a statement fails.
 */
[[nodiscard]] types::search_response search(io::connection& c,
    const types::view_filter& v, const types::search_request& r,
    const types::ranking& how = {});

/**
 * @brief The values of each filter over the documents @p r matches.
 * @throws unknown_environment when the environment does not exist.
 * @throws io::error when a statement fails.
 */
[[nodiscard]] types::facets_response facets(io::connection& c,
    const types::view_filter& v, const types::facets_request& r);

}

#endif

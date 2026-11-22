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
#ifndef COMPASS_DB_QUERY_TESTING_HPP
#define COMPASS_DB_QUERY_TESTING_HPP

#include <stdexcept>
#include <string>
#include "compass.db/io/connection.hpp"
#include "compass.db/query/search.hpp"
#include "compass.db/types/testing.hpp"

namespace compass::db::query {

/**
 * @brief The request named no sprint and none is STARTED, or named one the
 * environment does not hold inside the caller's view.
 */
class unknown_sprint : public std::runtime_error {
public:
    explicit unknown_sprint(const std::string& what)
        : std::runtime_error(what) {}
};

/**
 * @brief The stories of @p r's sprint inside @p v, each scenario that
 * names one of them as its parent story, and each scenario's state from
 * its latest finished run in the environment, with the open note counts.
 * @throws unknown_environment when the environment does not exist.
 * @throws unknown_sprint when there is no such sprint.
 * @throws io::error when a statement fails.
 */
[[nodiscard]] types::testing_response what_needs_testing(io::connection& c,
    const types::view_filter& v, const types::sprint_request& r);

/**
 * @brief The stories of @p r's sprint inside @p v and the tasks of each,
 * with their states.
 * @throws unknown_environment when the environment does not exist.
 * @throws unknown_sprint when there is no such sprint.
 * @throws io::error when a statement fails.
 */
[[nodiscard]] types::sprint_state_response sprint_state(io::connection& c,
    const types::view_filter& v, const types::sprint_request& r);

}

#endif

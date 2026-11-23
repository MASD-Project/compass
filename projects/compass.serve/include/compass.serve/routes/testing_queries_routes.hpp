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
/**
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Template: masd_cpp_http_routes.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_ROUTES_TESTING_QUERIES_ROUTES_HPP
#define COMPASS_SERVE_ROUTES_TESTING_QUERIES_ROUTES_HPP

#include <functional>
#include "compass.serve/domain/caller.hpp"
#include "compass.serve/messages/testing_queries_protocol.hpp"
#include "compass.serve/net/router.hpp"

namespace compass::serve::routes {

/**
 * @brief What answers get_sprint_state: Show a sprint's state.
 */
using get_sprint_state_handler = std::function<compass::serve::messages::sprint_state_response(
    const domain::caller&, const compass::serve::messages::sprint_state_request&)>;

/**
 * @brief Adds GET /v1/{environment}/sprint, the tool get_sprint_state, to @p r, answered by
 * @p h.
 */
void add_get_sprint_state(net::router& r, get_sprint_state_handler h);

/**
 * @brief What answers list_scenarios_to_test: Show what needs testing.
 */
using list_scenarios_to_test_handler = std::function<compass::serve::messages::testing_response(
    const domain::caller&, const compass::serve::messages::what_needs_testing_request&)>;

/**
 * @brief Adds GET /v1/{environment}/testing, the tool list_scenarios_to_test, to @p r, answered by
 * @p h.
 */
void add_list_scenarios_to_test(net::router& r, list_scenarios_to_test_handler h);

/**
 * @brief What answers get_run: Read a test run.
 */
using get_run_handler = std::function<compass::serve::messages::get_run_response(
    const domain::caller&, const compass::serve::messages::get_run_request&)>;

/**
 * @brief Adds GET /v1/{environment}/runs/{run}, the tool get_run, to @p r, answered by
 * @p h.
 */
void add_get_run(net::router& r, get_run_handler h);

/**
 * @brief What answers list_notes: List a campaign's notes.
 */
using list_notes_handler = std::function<compass::serve::messages::list_notes_response(
    const domain::caller&, const compass::serve::messages::list_notes_request&)>;

/**
 * @brief Adds GET /v1/{environment}/campaigns/{campaign}/notes, the tool list_notes, to @p r, answered by
 * @p h.
 */
void add_list_notes(net::router& r, list_notes_handler h);

}

#endif

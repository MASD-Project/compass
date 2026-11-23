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
#ifndef COMPASS_SERVE_ROUTES_CAMPAIGN_WRITES_ROUTES_HPP
#define COMPASS_SERVE_ROUTES_CAMPAIGN_WRITES_ROUTES_HPP

#include <functional>
#include "compass.serve/domain/caller.hpp"
#include "compass.serve/messages/campaign_writes_protocol.hpp"
#include "compass.serve/net/router.hpp"

namespace compass::serve::routes {

/**
 * @brief What answers start_run: Start a test run.
 */
using start_run_handler = std::function<compass::serve::messages::start_run_response(
    const domain::caller&, const compass::serve::messages::start_run_request&)>;

/**
 * @brief Adds POST /v1/{environment}/runs, the tool start_run, to @p r, answered by
 * @p h.
 */
void add_start_run(net::router& r, start_run_handler h);

/**
 * @brief What answers record_step_result: Record a step's outcome.
 */
using record_step_result_handler = std::function<void(
    const domain::caller&, const compass::serve::messages::step_result_request&)>;

/**
 * @brief Adds PUT /v1/{environment}/runs/{run}/steps/{step_ord}, the tool record_step_result, to @p r, answered by
 * @p h.
 */
void add_record_step_result(net::router& r, record_step_result_handler h);

/**
 * @brief What answers finish_run: Finish a test run.
 */
using finish_run_handler = std::function<compass::serve::messages::finish_run_response(
    const domain::caller&, const compass::serve::messages::finish_run_request&)>;

/**
 * @brief Adds POST /v1/{environment}/runs/{run}/finish, the tool finish_run, to @p r, answered by
 * @p h.
 */
void add_finish_run(net::router& r, finish_run_handler h);

/**
 * @brief What answers attach_screenshot: Attach a screenshot.
 */
using attach_screenshot_handler = std::function<compass::serve::messages::attach_response(
    const domain::caller&, const compass::serve::messages::attach_request&)>;

/**
 * @brief Adds POST /v1/{environment}/attachments, the tool attach_screenshot, to @p r, answered by
 * @p h.
 */
void add_attach_screenshot(net::router& r, attach_screenshot_handler h);

/**
 * @brief What answers add_note: Add a note to a campaign.
 */
using add_note_handler = std::function<compass::serve::messages::add_note_response(
    const domain::caller&, const compass::serve::messages::add_note_request&)>;

/**
 * @brief Adds POST /v1/{environment}/campaigns/{campaign}/notes, the tool add_note, to @p r, answered by
 * @p h.
 */
void add_add_note(net::router& r, add_note_handler h);

/**
 * @brief What answers address_note: Address a note.
 */
using address_note_handler = std::function<void(
    const domain::caller&, const compass::serve::messages::address_note_request&)>;

/**
 * @brief Adds PATCH /v1/{environment}/notes/{note}, the tool address_note, to @p r, answered by
 * @p h.
 */
void add_address_note(net::router& r, address_note_handler h);

}

#endif

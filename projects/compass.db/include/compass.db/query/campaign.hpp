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
#ifndef COMPASS_DB_QUERY_CAMPAIGN_HPP
#define COMPASS_DB_QUERY_CAMPAIGN_HPP

#include <cstdint>
#include <stdexcept>
#include <string>
#include "compass.db/io/connection.hpp"
#include "compass.db/query/search.hpp"
#include "compass.db/types/campaign.hpp"

namespace compass::db::query {

/**
 * @brief A campaign write the service refuses, and why: the request is
 * not valid in itself, names a row that does not exist, or conflicts with
 * the row's state, such as a write to a finished run. Nothing is written.
 */
class rejected_write : public std::runtime_error {
public:
    enum class reason { invalid, not_found, conflict };

    rejected_write(reason why, const std::string& what)
        : std::runtime_error(what), why_(why) {}

    [[nodiscard]] reason why() const noexcept { return why_; }

private:
    reason why_;
};

/*
 * Each write runs in one transaction and names its tester; a request whose
 * tester is empty or blank is invalid. Each throws rejected_write when it
 * is refused and io::error when a statement fails.
 */

/**
 * @brief Starts a run of the scenario as @p r's environment holds it,
 * opening the campaign of its parent story there on the first run.
 * @throws unknown_environment when the environment does not exist.
 */
types::start_run_response start_run(io::connection& c,
    const types::start_run_request& r);

/**
 * @brief Records, or replaces, the result of a step of an unfinished run.
 */
void record_step_result(io::connection& c,
    const types::step_result_request& r);

/**
 * @brief Attaches a screenshot to a run, at one of its steps or to the
 * run as a whole, or to a note, storing its bytes once by their blob ID.
 * Attaching the same bytes to the same place again changes only the
 * caption, when one is given.
 */
types::attach_response attach_screenshot(io::connection& c,
    const types::attach_request& r);

/**
 * @brief Adds an open note to a campaign, of one of @p kinds.
 * @returns the note's id.
 */
std::int64_t add_note(io::connection& c, const types::note_kinds& kinds,
    const types::add_note_request& r);

/**
 * @brief Marks an open note addressed.
 */
void address_note(io::connection& c, const types::address_note_request& r);

/**
 * @brief Finishes an unfinished run with the outcome its step results
 * give, as ops::derive_run_outcome derives it.
 */
types::finish_run_response finish_run(io::connection& c,
    const types::finish_run_request& r);

}

#endif

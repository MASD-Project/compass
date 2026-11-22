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
#ifndef COMPASS_DB_TYPES_CAMPAIGN_HPP
#define COMPASS_DB_TYPES_CAMPAIGN_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace compass::db::types {

/**
 * @brief The kinds a campaign note may have, the project's data: the
 * first column of the table under "* Note kinds" in its compass
 * configuration, .compass.org, read by whoever serves the requests.
 */
struct note_kinds {
    std::vector<std::string> names;
};

/**
 * @brief The outcome a tester gives one step of a run.
 */
enum class step_outcome { pass, fail, blocked, skipped };

/**
 * @brief The outcome of a finished run, derived from its steps' outcomes.
 */
enum class run_outcome { passed, failed, blocked };

/**
 * @brief Each outcome's name as the activity tables store it.
 */
[[nodiscard]] std::string_view to_string(step_outcome o) noexcept;
[[nodiscard]] std::string_view to_string(run_outcome o) noexcept;

/**
 * @brief Starts a run of the scenario whose :ID: is scenario_id, as the
 * environment holds it now, by tester. The first run of a story's
 * scenarios in an environment opens the story's campaign there.
 */
struct start_run_request {
    std::string environment;
    std::string scenario_id;
    std::string tester;
};

/**
 * @brief A step of the definition a run follows: its ord, the step's
 * place under * Steps from 0, its title and its body.
 */
struct run_step {
    std::int32_t ord = 0;
    std::string title;
    std::string body;
};

/**
 * @brief The run started, its campaign, the definition and document it
 * follows, as hex blob IDs, and that definition's steps, so a tester never
 * sees a step list mixed between versions.
 */
struct start_run_response {
    std::int64_t run = 0;
    std::int64_t campaign = 0;
    std::string definition;
    std::string scenario_hash;
    std::string started_at;
    std::vector<run_step> steps;
};

/**
 * @brief The outcome of one step of an unfinished run, with the tester's
 * notes. Recording a step again replaces its result.
 */
struct step_result_request {
    std::int64_t run = 0;
    std::int32_t step_ord = 0;
    step_outcome outcome = step_outcome::pass;
    std::optional<std::string> notes;
    std::string tester;
};

/**
 * @brief A screenshot attached to a run, optionally at one of its steps,
 * or to a note: exactly one of run and note is set, and step_ord only with
 * run. bytes are the image, of media_type, an image type.
 */
struct attach_request {
    std::optional<std::int64_t> run;
    std::optional<std::int64_t> note;
    std::optional<std::int32_t> step_ord;
    std::string media_type;
    std::string bytes;
    std::optional<std::string> caption;
    std::string tester;
};

/**
 * @brief Where the screenshot is attached and the hex blob ID it is
 * stored under; the same bytes are stored once, however often attached.
 */
struct attach_response {
    std::int64_t attached = 0;
    std::string attachment;
};

/**
 * @brief A note on a campaign, optionally naming one of its story's
 * scenarios and, with it, one of that scenario's steps; kind is one of the
 * project's note kinds.
 */
struct add_note_request {
    std::int64_t campaign = 0;
    std::optional<std::string> scenario_id;
    std::optional<std::int32_t> step_ord;
    std::string kind;
    std::string text;
    std::string tester;
};

/**
 * @brief Marks an open note addressed by tester, with an optional reply.
 */
struct address_note_request {
    std::int64_t note = 0;
    std::optional<std::string> reply;
    std::string tester;
};

/**
 * @brief Finishes an unfinished run.
 */
struct finish_run_request {
    std::int64_t run = 0;
    std::string tester;
};

/**
 * @brief The finished run's derived outcome and when it finished.
 */
struct finish_run_response {
    run_outcome outcome = run_outcome::passed;
    std::string finished_at;
};

}

#endif

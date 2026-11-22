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
#ifndef COMPASS_DB_TYPES_TESTING_HPP
#define COMPASS_DB_TYPES_TESTING_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace compass::db::types {

/**
 * @brief Where a scenario stands in an environment, from its latest
 * finished run there. Untested, failed and changed need testing; passed
 * is done.
 *
 * - untested: no run of it has finished.
 * - failed: the latest run failed or was blocked.
 * - passed: the latest run passed against the scenario's current
 *   definition.
 * - changed: the latest run passed against an earlier definition.
 */
enum class scenario_state { untested, failed, passed, changed };

/**
 * @brief The state's name as this header spells it.
 */
[[nodiscard]] std::string_view to_string(scenario_state s) noexcept;

/**
 * @brief A sprint: its :ID:, title and the State row of its Status table.
 */
struct sprint_summary {
    std::string org_id;
    std::string title;
    std::string state;
};

/**
 * @brief Which sprint of an environment a request is about. sprint is a
 * sprint page's :ID:, its title or the name of the directory that holds
 * it; when it is not set, the sprint is the present one, the sprint whose
 * state is STARTED, the latest created when several are.
 */
struct sprint_request {
    std::string environment;
    std::optional<std::string> sprint;
};

/**
 * @brief The latest finished run of a scenario in an environment: who ran
 * it, when it finished, its outcome (passed, failed or blocked) and
 * whether it ran the scenario's current definition.
 */
struct run_summary {
    std::int64_t id = 0;
    std::string tester;
    std::string finished_at;
    std::string outcome;
    bool current_definition = false;
};

/**
 * @brief One scenario of a story: its page, the task it verifies, its
 * state, its latest finished run when it has one and how many notes
 * naming it are open in the environment.
 */
struct scenario_status {
    std::string org_id;
    std::string title;
    std::string path;
    std::string task_id;
    scenario_state state = scenario_state::untested;
    std::optional<run_summary> latest_run;
    std::uint64_t open_notes = 0;
};

/**
 * @brief One story of the sprint and its scenarios. campaign is the id of
 * the story's campaign in the environment, once a run has opened one;
 * open_notes counts every open note of that campaign, those that name no
 * scenario included.
 */
struct story_testing {
    std::string org_id;
    std::string title;
    std::string state;
    std::string path;
    std::optional<std::int64_t> campaign;
    std::uint64_t open_notes = 0;
    std::vector<scenario_status> scenarios;
};

/**
 * @brief What needs testing in a sprint: its stories in path order, each
 * with its scenarios in path order, and when the environment was last
 * synced.
 */
struct testing_response {
    sprint_summary sprint;
    std::vector<story_testing> stories;
    std::string synced_at;
};

/**
 * @brief A story or task: its page and the State row of its Status table,
 * empty when it has none.
 */
struct work_item {
    std::string org_id;
    std::string title;
    std::string state;
    std::string path;
};

/**
 * @brief A story of the sprint and, in path order, the tasks whose
 * preamble names it.
 */
struct story_state {
    work_item story;
    std::vector<work_item> tasks;
};

/**
 * @brief A sprint's stories in path order, with their tasks, and when the
 * environment was last synced.
 */
struct sprint_state_response {
    sprint_summary sprint;
    std::vector<story_state> stories;
    std::string synced_at;
};

}

#endif

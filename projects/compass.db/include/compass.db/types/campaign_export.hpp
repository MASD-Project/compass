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
#ifndef COMPASS_DB_TYPES_CAMPAIGN_EXPORT_HPP
#define COMPASS_DB_TYPES_CAMPAIGN_EXPORT_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace compass::db::types {

/**
 * @brief A screenshot attached to a run or a note: the hex blob ID it is
 * stored under, whose exported_image holds its bytes and media type, the
 * step it shows when it was attached at one, and its caption.
 */
struct exported_screenshot {
    std::string attachment;
    std::optional<std::int32_t> step_ord;
    std::optional<std::string> caption;
};

/**
 * @brief A step of the definition a run followed, by ord from 0, with the
 * result the tester recorded for it, if any.
 */
struct exported_step {
    std::int32_t ord = 0;
    std::string title;
    std::optional<std::string> outcome;
    std::optional<std::string> notes;
};

/**
 * @brief When a run finished and the outcome derived then.
 */
struct run_finish {
    std::string at;
    std::string outcome;
};

/**
 * @brief One run of a scenario: who ran it, when, against which
 * definition (a hex blob ID), its steps and its screenshots. finished is
 * empty while the run is in progress.
 */
struct exported_run {
    std::int64_t id = 0;
    std::string tester;
    std::string started_at;
    std::optional<run_finish> finished;
    std::string definition;
    std::vector<exported_step> steps;
    std::vector<exported_screenshot> screenshots;
};

/**
 * @brief A scenario the campaign ran or a note names: its :ID:, its title
 * as the environment holds it, and its runs in the order they started.
 */
struct exported_scenario {
    std::string org_id;
    std::string title;
    std::vector<exported_run> runs;
};

/**
 * @brief Who addressed a note, when, and the reply they gave.
 */
struct note_address {
    std::string by;
    std::string at;
    std::optional<std::string> reply;
};

/**
 * @brief A note on the campaign, optionally naming a scenario and one of
 * its steps. addressed is empty while the note is open.
 */
struct exported_note {
    std::int64_t id = 0;
    std::optional<std::string> scenario_id;
    std::optional<std::int32_t> step_ord;
    std::string kind;
    std::string text;
    std::string author;
    std::string created_at;
    std::optional<note_address> addressed;
    std::vector<exported_screenshot> screenshots;
};

/**
 * @brief The bytes of a screenshot, stored once under its hex blob ID.
 */
struct exported_image {
    std::string attachment;
    std::string media_type;
    std::string bytes;
};

/**
 * @brief A page a campaign names: its :ID: and its title.
 */
struct page_ref {
    std::string org_id;
    std::string title;
};

/**
 * @brief Everything the export of one campaign reads: the campaign, its
 * story's page (path relative to the environment's root), its sprint
 * (its :ID: as its title when the environment does not hold it), its
 * scenarios with their runs, its notes and the screenshots they show.
 * Scenarios are ordered by title, then :ID:; runs and notes by id. Times
 * are UTC, written YYYY-MM-DDTHH:MM:SSZ.
 */
struct campaign_rows {
    std::int64_t id = 0;
    std::string environment;
    std::string story_id;
    std::string story_title;
    std::string story_path;
    std::optional<page_ref> sprint;
    std::string opened_at;
    std::vector<exported_scenario> scenarios;
    std::vector<exported_note> notes;
    std::vector<exported_image> images;
};

/**
 * @brief The campaigns one export reads, and the time of the snapshot it
 * read them in, as PostgreSQL writes a timestamptz.
 */
struct campaign_snapshot {
    std::string taken_at;
    std::vector<campaign_rows> campaigns;
};

/**
 * @brief A file the export writes: its path, relative to the checkout,
 * and its exact bytes.
 */
struct exported_file {
    std::string path;
    std::string bytes;
};

}

#endif

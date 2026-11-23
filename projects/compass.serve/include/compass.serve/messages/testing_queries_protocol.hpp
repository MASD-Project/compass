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
 * Template: masd_cpp_protocol.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_MESSAGES_TESTING_QUERIES_PROTOCOL_HPP
#define COMPASS_SERVE_MESSAGES_TESTING_QUERIES_PROTOCOL_HPP

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include "compass.db/codec/text_form.hpp"
#include "compass.db/codec/wire_fields.hpp"
#include "compass.db/codec/wire_traits_fwd.hpp"

namespace compass::serve::messages {

#ifndef COMPASS_SERVE_MESSAGES_SCENARIO_STATE_ENUM
/**
 * @brief The values :values: lists for scenario_state.
 */
enum class scenario_state { untested, failed, passed, changed };

/**
 * @brief Each value of scenario_state as the wire names it.
 */
[[nodiscard]] constexpr std::string_view to_string(scenario_state v) noexcept {
    switch (v) {
    case scenario_state::untested: return "untested";
    case scenario_state::failed: return "failed";
    case scenario_state::passed: return "passed";
    case scenario_state::changed: return "changed";
    }
    return "untested";
}

/**
 * @brief The scenario_state @p text names, or none for any other text.
 */
[[nodiscard]] constexpr std::optional<scenario_state> scenario_state_of(
    std::string_view text) noexcept {
    if (text == "untested")
        return scenario_state::untested;
    if (text == "failed")
        return scenario_state::failed;
    if (text == "passed")
        return scenario_state::passed;
    if (text == "changed")
        return scenario_state::changed;
    return std::nullopt;
}
#endif

#ifndef COMPASS_SERVE_MESSAGES_STEP_OUTCOME_ENUM
/**
 * @brief The values :values: lists for step_outcome.
 */
enum class step_outcome { pass, fail, blocked, skipped };

/**
 * @brief Each value of step_outcome as the wire names it.
 */
[[nodiscard]] constexpr std::string_view to_string(step_outcome v) noexcept {
    switch (v) {
    case step_outcome::pass: return "pass";
    case step_outcome::fail: return "fail";
    case step_outcome::blocked: return "blocked";
    case step_outcome::skipped: return "skipped";
    }
    return "pass";
}

/**
 * @brief The step_outcome @p text names, or none for any other text.
 */
[[nodiscard]] constexpr std::optional<step_outcome> step_outcome_of(
    std::string_view text) noexcept {
    if (text == "pass")
        return step_outcome::pass;
    if (text == "fail")
        return step_outcome::fail;
    if (text == "blocked")
        return step_outcome::blocked;
    if (text == "skipped")
        return step_outcome::skipped;
    return std::nullopt;
}
#endif

struct sprint_state_request {
    std::string environment;
    std::optional<std::string> sprint;

    friend bool operator==(const sprint_state_request&, const sprint_state_request&) = default;
};

/**
 * @brief The members of sprint_state_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<sprint_state_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &sprint_state_request::environment},
        ::compass::db::codec::field{"sprint", &sprint_state_request::sprint},
    };
}

struct what_needs_testing_request {
    std::string environment;
    std::optional<std::string> sprint;

    friend bool operator==(const what_needs_testing_request&, const what_needs_testing_request&) = default;
};

/**
 * @brief The members of what_needs_testing_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<what_needs_testing_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &what_needs_testing_request::environment},
        ::compass::db::codec::field{"sprint", &what_needs_testing_request::sprint},
    };
}

struct sprint_summary {
    std::string org_id;
    std::string title;
    std::string state;

    friend bool operator==(const sprint_summary&, const sprint_summary&) = default;
};

/**
 * @brief The members of sprint_summary, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<sprint_summary>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"org_id", &sprint_summary::org_id},
        ::compass::db::codec::field{"title", &sprint_summary::title},
        ::compass::db::codec::field{"state", &sprint_summary::state},
    };
}

struct work_item {
    std::string org_id;
    std::string title;
    std::string state;
    std::string path;

    friend bool operator==(const work_item&, const work_item&) = default;
};

/**
 * @brief The members of work_item, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<work_item>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"org_id", &work_item::org_id},
        ::compass::db::codec::field{"title", &work_item::title},
        ::compass::db::codec::field{"state", &work_item::state},
        ::compass::db::codec::field{"path", &work_item::path},
    };
}

struct story_state {
    work_item story;
    std::vector<work_item> tasks;

    friend bool operator==(const story_state&, const story_state&) = default;
};

/**
 * @brief The members of story_state, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<story_state>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"story", &story_state::story},
        ::compass::db::codec::field{"tasks", &story_state::tasks},
    };
}

struct sprint_state_response {
    sprint_summary sprint;
    std::vector<story_state> stories;
    std::string synced_at;

    friend bool operator==(const sprint_state_response&, const sprint_state_response&) = default;
};

/**
 * @brief The members of sprint_state_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<sprint_state_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"sprint", &sprint_state_response::sprint},
        ::compass::db::codec::field{"stories", &sprint_state_response::stories},
        ::compass::db::codec::field{"synced_at", &sprint_state_response::synced_at},
    };
}

struct run_summary {
    std::int64_t id = 0;
    std::string tester;
    std::string finished_at;
    std::string outcome;
    bool current_definition = false;

    friend bool operator==(const run_summary&, const run_summary&) = default;
};

/**
 * @brief The members of run_summary, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<run_summary>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"id", &run_summary::id},
        ::compass::db::codec::field{"tester", &run_summary::tester},
        ::compass::db::codec::field{"finished_at", &run_summary::finished_at},
        ::compass::db::codec::field{"outcome", &run_summary::outcome},
        ::compass::db::codec::field{"current_definition", &run_summary::current_definition},
    };
}

struct scenario_status {
    std::string org_id;
    std::string title;
    std::string path;
    std::string task_id;
    scenario_state state = scenario_state::untested;
    std::optional<run_summary> latest_run;
    std::uint64_t open_notes = 0;

    friend bool operator==(const scenario_status&, const scenario_status&) = default;
};

/**
 * @brief The members of scenario_status, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<scenario_status>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"org_id", &scenario_status::org_id},
        ::compass::db::codec::field{"title", &scenario_status::title},
        ::compass::db::codec::field{"path", &scenario_status::path},
        ::compass::db::codec::field{"task_id", &scenario_status::task_id},
        ::compass::db::codec::field{"state", &scenario_status::state},
        ::compass::db::codec::field{"latest_run", &scenario_status::latest_run},
        ::compass::db::codec::field{"open_notes", &scenario_status::open_notes},
    };
}

struct story_testing {
    std::string org_id;
    std::string title;
    std::string state;
    std::string path;
    std::optional<std::int64_t> campaign;
    std::uint64_t open_notes = 0;
    std::vector<scenario_status> scenarios;

    friend bool operator==(const story_testing&, const story_testing&) = default;
};

/**
 * @brief The members of story_testing, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<story_testing>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"org_id", &story_testing::org_id},
        ::compass::db::codec::field{"title", &story_testing::title},
        ::compass::db::codec::field{"state", &story_testing::state},
        ::compass::db::codec::field{"path", &story_testing::path},
        ::compass::db::codec::field{"campaign", &story_testing::campaign},
        ::compass::db::codec::field{"open_notes", &story_testing::open_notes},
        ::compass::db::codec::field{"scenarios", &story_testing::scenarios},
    };
}

struct testing_response {
    sprint_summary sprint;
    std::vector<story_testing> stories;
    std::string synced_at;

    friend bool operator==(const testing_response&, const testing_response&) = default;
};

/**
 * @brief The members of testing_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<testing_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"sprint", &testing_response::sprint},
        ::compass::db::codec::field{"stories", &testing_response::stories},
        ::compass::db::codec::field{"synced_at", &testing_response::synced_at},
    };
}

struct get_run_request {
    std::string environment;
    std::int64_t run = 0;

    friend bool operator==(const get_run_request&, const get_run_request&) = default;
};

/**
 * @brief The members of get_run_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<get_run_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &get_run_request::environment},
        ::compass::db::codec::field{"run", &get_run_request::run},
    };
}

struct run_finish {
    std::string at;
    std::string outcome;

    friend bool operator==(const run_finish&, const run_finish&) = default;
};

/**
 * @brief The members of run_finish, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<run_finish>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"at", &run_finish::at},
        ::compass::db::codec::field{"outcome", &run_finish::outcome},
    };
}

struct step_result {
    std::int32_t ord = 0;
    std::string title;
    std::string body;
    std::optional<step_outcome> outcome;
    std::optional<std::string> notes;

    friend bool operator==(const step_result&, const step_result&) = default;
};

/**
 * @brief The members of step_result, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<step_result>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"ord", &step_result::ord},
        ::compass::db::codec::field{"title", &step_result::title},
        ::compass::db::codec::field{"body", &step_result::body},
        ::compass::db::codec::field{"outcome", &step_result::outcome},
        ::compass::db::codec::field{"notes", &step_result::notes},
    };
}

struct get_run_response {
    std::int64_t id = 0;
    std::int64_t campaign = 0;
    std::string scenario_id;
    std::string tester;
    std::string definition;
    std::string started_at;
    std::optional<run_finish> finished;
    std::vector<step_result> steps;

    friend bool operator==(const get_run_response&, const get_run_response&) = default;
};

/**
 * @brief The members of get_run_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<get_run_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"id", &get_run_response::id},
        ::compass::db::codec::field{"campaign", &get_run_response::campaign},
        ::compass::db::codec::field{"scenario_id", &get_run_response::scenario_id},
        ::compass::db::codec::field{"tester", &get_run_response::tester},
        ::compass::db::codec::field{"definition", &get_run_response::definition},
        ::compass::db::codec::field{"started_at", &get_run_response::started_at},
        ::compass::db::codec::field{"finished", &get_run_response::finished},
        ::compass::db::codec::field{"steps", &get_run_response::steps},
    };
}

struct list_notes_request {
    std::string environment;
    std::int64_t campaign = 0;

    friend bool operator==(const list_notes_request&, const list_notes_request&) = default;
};

/**
 * @brief The members of list_notes_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<list_notes_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &list_notes_request::environment},
        ::compass::db::codec::field{"campaign", &list_notes_request::campaign},
    };
}

struct note_address {
    std::string by;
    std::string at;
    std::optional<std::string> reply;

    friend bool operator==(const note_address&, const note_address&) = default;
};

/**
 * @brief The members of note_address, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<note_address>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"by", &note_address::by},
        ::compass::db::codec::field{"at", &note_address::at},
        ::compass::db::codec::field{"reply", &note_address::reply},
    };
}

struct campaign_note {
    std::int64_t id = 0;
    std::optional<std::string> scenario_id;
    std::optional<std::int32_t> step_ord;
    std::string kind;
    std::string text;
    std::string author;
    std::string created_at;
    std::optional<note_address> addressed;

    friend bool operator==(const campaign_note&, const campaign_note&) = default;
};

/**
 * @brief The members of campaign_note, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<campaign_note>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"id", &campaign_note::id},
        ::compass::db::codec::field{"scenario_id", &campaign_note::scenario_id},
        ::compass::db::codec::field{"step_ord", &campaign_note::step_ord},
        ::compass::db::codec::field{"kind", &campaign_note::kind},
        ::compass::db::codec::field{"text", &campaign_note::text},
        ::compass::db::codec::field{"author", &campaign_note::author},
        ::compass::db::codec::field{"created_at", &campaign_note::created_at},
        ::compass::db::codec::field{"addressed", &campaign_note::addressed},
    };
}

struct list_notes_response {
    std::vector<campaign_note> notes;

    friend bool operator==(const list_notes_response&, const list_notes_response&) = default;
};

/**
 * @brief The members of list_notes_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<list_notes_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"notes", &list_notes_response::notes},
    };
}

}

namespace compass::db::codec {

#ifndef COMPASS_SERVE_MESSAGES_SCENARIO_STATE_ENUM
#define COMPASS_SERVE_MESSAGES_SCENARIO_STATE_ENUM
/**
 * @brief scenario_state travels as the name of its value.
 */
template <>
struct text_form<compass::serve::messages::scenario_state> {
    static constexpr std::string_view wire_name = "a scenario_state";

    static constexpr std::string_view names[] = { "untested", "failed", "passed", "changed" };

    static constexpr compass::serve::messages::scenario_state values[] = { compass::serve::messages::scenario_state::untested, compass::serve::messages::scenario_state::failed, compass::serve::messages::scenario_state::passed, compass::serve::messages::scenario_state::changed };

    static std::string to_text(compass::serve::messages::scenario_state v) {
        return std::string(to_string(v));
    }

    static compass::serve::messages::scenario_state from_text(std::string_view text) {
        if (const auto v = compass::serve::messages::scenario_state_of(text))
            return *v;
        throw std::invalid_argument("Not " + std::string(wire_name) + ": '" +
                                    std::string(text) + "' is not one of untested, failed, passed, changed");
    }
};
#endif

#ifndef COMPASS_SERVE_MESSAGES_STEP_OUTCOME_ENUM
#define COMPASS_SERVE_MESSAGES_STEP_OUTCOME_ENUM
/**
 * @brief step_outcome travels as the name of its value.
 */
template <>
struct text_form<compass::serve::messages::step_outcome> {
    static constexpr std::string_view wire_name = "a step_outcome";

    static constexpr std::string_view names[] = { "pass", "fail", "blocked", "skipped" };

    static constexpr compass::serve::messages::step_outcome values[] = { compass::serve::messages::step_outcome::pass, compass::serve::messages::step_outcome::fail, compass::serve::messages::step_outcome::blocked, compass::serve::messages::step_outcome::skipped };

    static std::string to_text(compass::serve::messages::step_outcome v) {
        return std::string(to_string(v));
    }

    static compass::serve::messages::step_outcome from_text(std::string_view text) {
        if (const auto v = compass::serve::messages::step_outcome_of(text))
            return *v;
        throw std::invalid_argument("Not " + std::string(wire_name) + ": '" +
                                    std::string(text) + "' is not one of pass, fail, blocked, skipped");
    }
};
#endif

template <>
struct wire_traits<compass::serve::messages::sprint_state_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::sprint_state_request& v);

    static compass::serve::messages::sprint_state_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::what_needs_testing_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::what_needs_testing_request& v);

    static compass::serve::messages::what_needs_testing_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::sprint_summary> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::sprint_summary& v);

    static compass::serve::messages::sprint_summary read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::work_item> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::work_item& v);

    static compass::serve::messages::work_item read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::story_state> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::story_state& v);

    static compass::serve::messages::story_state read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::sprint_state_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::sprint_state_response& v);

    static compass::serve::messages::sprint_state_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::run_summary> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::run_summary& v);

    static compass::serve::messages::run_summary read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::scenario_status> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::scenario_status& v);

    static compass::serve::messages::scenario_status read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::story_testing> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::story_testing& v);

    static compass::serve::messages::story_testing read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::testing_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::testing_response& v);

    static compass::serve::messages::testing_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::get_run_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::get_run_request& v);

    static compass::serve::messages::get_run_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::run_finish> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::run_finish& v);

    static compass::serve::messages::run_finish read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::step_result> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::step_result& v);

    static compass::serve::messages::step_result read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::get_run_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::get_run_response& v);

    static compass::serve::messages::get_run_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::list_notes_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::list_notes_request& v);

    static compass::serve::messages::list_notes_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::note_address> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::note_address& v);

    static compass::serve::messages::note_address read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::campaign_note> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::campaign_note& v);

    static compass::serve::messages::campaign_note read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::list_notes_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::list_notes_response& v);

    static compass::serve::messages::list_notes_response read_json(std::string_view text);
};

}

#endif

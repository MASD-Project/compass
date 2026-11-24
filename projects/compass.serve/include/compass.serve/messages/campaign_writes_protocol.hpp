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
#ifndef COMPASS_SERVE_MESSAGES_CAMPAIGN_WRITES_PROTOCOL_HPP
#define COMPASS_SERVE_MESSAGES_CAMPAIGN_WRITES_PROTOCOL_HPP

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include "compass.db/codec/base64.hpp"
#include "compass.db/codec/text_form.hpp"
#include "compass.db/codec/wire_fields.hpp"
#include "compass.db/codec/wire_traits_fwd.hpp"

namespace compass::serve::messages {

#ifndef COMPASS_SERVE_MESSAGES_RUN_OUTCOME_ENUM
/**
 * @brief The values :values: lists for run_outcome.
 */
enum class run_outcome { passed, failed, blocked };

/**
 * @brief Each value of run_outcome as the wire names it.
 */
[[nodiscard]] constexpr std::string_view to_string(run_outcome v) {
    switch (v) {
    case run_outcome::passed: return "passed";
    case run_outcome::failed: return "failed";
    case run_outcome::blocked: return "blocked";
    }
    throw std::invalid_argument("Not a run_outcome: the value is not one of passed, failed, blocked");
}

/**
 * @brief The run_outcome @p text names, or none for any other text.
 */
[[nodiscard]] constexpr std::optional<run_outcome> run_outcome_of(
    std::string_view text) noexcept {
    if (text == "passed")
        return run_outcome::passed;
    if (text == "failed")
        return run_outcome::failed;
    if (text == "blocked")
        return run_outcome::blocked;
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
[[nodiscard]] constexpr std::string_view to_string(step_outcome v) {
    switch (v) {
    case step_outcome::pass: return "pass";
    case step_outcome::fail: return "fail";
    case step_outcome::blocked: return "blocked";
    case step_outcome::skipped: return "skipped";
    }
    throw std::invalid_argument("Not a step_outcome: the value is not one of pass, fail, blocked, skipped");
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

struct start_run_request {
    std::string environment;
    std::string scenario_id;

    friend bool operator==(const start_run_request&, const start_run_request&) = default;
};

/**
 * @brief The members of start_run_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<start_run_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &start_run_request::environment},
        ::compass::db::codec::field{"scenario_id", &start_run_request::scenario_id},
    };
}

/**
 * @brief The members of start_run_request its route reads from the JSON body. The
 * route sets the others from the request line.
 */
[[nodiscard]] constexpr auto body_fields(std::type_identity<start_run_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"scenario_id", &start_run_request::scenario_id},
    };
}

struct run_step {
    std::int32_t ord = 0;
    std::string title;
    std::string body;

    friend bool operator==(const run_step&, const run_step&) = default;
};

/**
 * @brief The members of run_step, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<run_step>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"ord", &run_step::ord, true},
        ::compass::db::codec::field{"title", &run_step::title},
        ::compass::db::codec::field{"body", &run_step::body},
    };
}

struct start_run_response {
    std::int64_t run = 0;
    std::int64_t campaign = 0;
    std::string definition;
    std::string scenario_hash;
    std::string started_at;
    std::vector<run_step> steps;

    friend bool operator==(const start_run_response&, const start_run_response&) = default;
};

/**
 * @brief The members of start_run_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<start_run_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"run", &start_run_response::run, true},
        ::compass::db::codec::field{"campaign", &start_run_response::campaign, true},
        ::compass::db::codec::field{"definition", &start_run_response::definition},
        ::compass::db::codec::field{"scenario_hash", &start_run_response::scenario_hash},
        ::compass::db::codec::field{"started_at", &start_run_response::started_at},
        ::compass::db::codec::field{"steps", &start_run_response::steps, true},
    };
}

struct step_result_request {
    std::string environment;
    std::int64_t run = 0;
    std::int32_t step_ord = 0;
    step_outcome outcome = step_outcome::pass;
    std::optional<std::string> notes;

    friend bool operator==(const step_result_request&, const step_result_request&) = default;
};

/**
 * @brief The members of step_result_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<step_result_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &step_result_request::environment},
        ::compass::db::codec::field{"run", &step_result_request::run},
        ::compass::db::codec::field{"step_ord", &step_result_request::step_ord},
        ::compass::db::codec::field{"outcome", &step_result_request::outcome, true},
        ::compass::db::codec::field{"notes", &step_result_request::notes},
    };
}

/**
 * @brief The members of step_result_request its route reads from the JSON body. The
 * route sets the others from the request line.
 */
[[nodiscard]] constexpr auto body_fields(std::type_identity<step_result_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"outcome", &step_result_request::outcome, true},
        ::compass::db::codec::field{"notes", &step_result_request::notes},
    };
}

struct finish_run_request {
    std::string environment;
    std::int64_t run = 0;

    friend bool operator==(const finish_run_request&, const finish_run_request&) = default;
};

/**
 * @brief The members of finish_run_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<finish_run_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &finish_run_request::environment},
        ::compass::db::codec::field{"run", &finish_run_request::run},
    };
}

struct finish_run_response {
    run_outcome outcome = run_outcome::passed;
    std::string finished_at;

    friend bool operator==(const finish_run_response&, const finish_run_response&) = default;
};

/**
 * @brief The members of finish_run_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<finish_run_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"outcome", &finish_run_response::outcome, true},
        ::compass::db::codec::field{"finished_at", &finish_run_response::finished_at},
    };
}

struct attach_request {
    std::string environment;
    std::optional<std::int64_t> run;
    std::optional<std::int64_t> note;
    std::optional<std::int32_t> step_ord;
    std::string media_type;
    compass::db::codec::base64_bytes bytes;
    std::optional<std::string> caption;

    friend bool operator==(const attach_request&, const attach_request&) = default;
};

/**
 * @brief The members of attach_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<attach_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &attach_request::environment},
        ::compass::db::codec::field{"run", &attach_request::run},
        ::compass::db::codec::field{"note", &attach_request::note},
        ::compass::db::codec::field{"step_ord", &attach_request::step_ord},
        ::compass::db::codec::field{"media_type", &attach_request::media_type},
        ::compass::db::codec::field{"bytes", &attach_request::bytes},
        ::compass::db::codec::field{"caption", &attach_request::caption},
    };
}

/**
 * @brief The members of attach_request its route reads from the JSON body. The
 * route sets the others from the request line.
 */
[[nodiscard]] constexpr auto body_fields(std::type_identity<attach_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"run", &attach_request::run},
        ::compass::db::codec::field{"note", &attach_request::note},
        ::compass::db::codec::field{"step_ord", &attach_request::step_ord},
        ::compass::db::codec::field{"media_type", &attach_request::media_type},
        ::compass::db::codec::field{"bytes", &attach_request::bytes},
        ::compass::db::codec::field{"caption", &attach_request::caption},
    };
}

struct attach_response {
    std::int64_t attached = 0;
    std::string attachment;

    friend bool operator==(const attach_response&, const attach_response&) = default;
};

/**
 * @brief The members of attach_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<attach_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"attached", &attach_response::attached, true},
        ::compass::db::codec::field{"attachment", &attach_response::attachment},
    };
}

struct add_note_request {
    std::string environment;
    std::int64_t campaign = 0;
    std::optional<std::string> scenario_id;
    std::optional<std::int32_t> step_ord;
    std::string kind;
    std::string text;

    friend bool operator==(const add_note_request&, const add_note_request&) = default;
};

/**
 * @brief The members of add_note_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<add_note_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &add_note_request::environment},
        ::compass::db::codec::field{"campaign", &add_note_request::campaign},
        ::compass::db::codec::field{"scenario_id", &add_note_request::scenario_id},
        ::compass::db::codec::field{"step_ord", &add_note_request::step_ord},
        ::compass::db::codec::field{"kind", &add_note_request::kind},
        ::compass::db::codec::field{"text", &add_note_request::text},
    };
}

/**
 * @brief The members of add_note_request its route reads from the JSON body. The
 * route sets the others from the request line.
 */
[[nodiscard]] constexpr auto body_fields(std::type_identity<add_note_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"scenario_id", &add_note_request::scenario_id},
        ::compass::db::codec::field{"step_ord", &add_note_request::step_ord},
        ::compass::db::codec::field{"kind", &add_note_request::kind},
        ::compass::db::codec::field{"text", &add_note_request::text},
    };
}

struct add_note_response {
    std::int64_t note = 0;

    friend bool operator==(const add_note_response&, const add_note_response&) = default;
};

/**
 * @brief The members of add_note_response, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<add_note_response>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"note", &add_note_response::note, true},
    };
}

struct address_note_request {
    std::string environment;
    std::int64_t note = 0;
    std::optional<std::string> reply;

    friend bool operator==(const address_note_request&, const address_note_request&) = default;
};

/**
 * @brief The members of address_note_request, named as the wire names them and in the
 * order the type declares them.
 */
[[nodiscard]] constexpr auto wire_fields(std::type_identity<address_note_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"environment", &address_note_request::environment},
        ::compass::db::codec::field{"note", &address_note_request::note},
        ::compass::db::codec::field{"reply", &address_note_request::reply},
    };
}

/**
 * @brief The members of address_note_request its route reads from the JSON body. The
 * route sets the others from the request line.
 */
[[nodiscard]] constexpr auto body_fields(std::type_identity<address_note_request>) {
    return ::compass::db::codec::field_list{
        ::compass::db::codec::field{"reply", &address_note_request::reply},
    };
}

}

namespace compass::db::codec {

#ifndef COMPASS_SERVE_MESSAGES_RUN_OUTCOME_ENUM
#define COMPASS_SERVE_MESSAGES_RUN_OUTCOME_ENUM
/**
 * @brief run_outcome travels as the name of its value.
 */
template <>
struct text_form<compass::serve::messages::run_outcome> {
    static constexpr std::string_view wire_name = "a run_outcome";

    static constexpr std::string_view names[] = { "passed", "failed", "blocked" };

    static constexpr compass::serve::messages::run_outcome values[] = { compass::serve::messages::run_outcome::passed, compass::serve::messages::run_outcome::failed, compass::serve::messages::run_outcome::blocked };

    static std::string to_text(compass::serve::messages::run_outcome v) {
        return std::string(to_string(v));
    }

    static compass::serve::messages::run_outcome from_text(std::string_view text) {
        if (const auto v = compass::serve::messages::run_outcome_of(text))
            return *v;
        throw std::invalid_argument("Not " + std::string(wire_name) + ": '" +
                                    std::string(text) + "' is not one of passed, failed, blocked");
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
struct wire_traits<compass::serve::messages::start_run_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::start_run_request& v);

    static compass::serve::messages::start_run_request read_json(std::string_view text);

    static compass::serve::messages::start_run_request read_body_json(std::string_view text);

    static std::string body_json_schema();
};

template <>
struct wire_traits<compass::serve::messages::run_step> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::run_step& v);

    static compass::serve::messages::run_step read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::start_run_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::start_run_response& v);

    static compass::serve::messages::start_run_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::step_result_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::step_result_request& v);

    static compass::serve::messages::step_result_request read_json(std::string_view text);

    static compass::serve::messages::step_result_request read_body_json(std::string_view text);

    static std::string body_json_schema();
};

template <>
struct wire_traits<compass::serve::messages::finish_run_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::finish_run_request& v);

    static compass::serve::messages::finish_run_request read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::finish_run_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::finish_run_response& v);

    static compass::serve::messages::finish_run_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::attach_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::attach_request& v);

    static compass::serve::messages::attach_request read_json(std::string_view text);

    static compass::serve::messages::attach_request read_body_json(std::string_view text);

    static std::string body_json_schema();
};

template <>
struct wire_traits<compass::serve::messages::attach_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::attach_response& v);

    static compass::serve::messages::attach_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::add_note_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::add_note_request& v);

    static compass::serve::messages::add_note_request read_json(std::string_view text);

    static compass::serve::messages::add_note_request read_body_json(std::string_view text);

    static std::string body_json_schema();
};

template <>
struct wire_traits<compass::serve::messages::add_note_response> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::add_note_response& v);

    static compass::serve::messages::add_note_response read_json(std::string_view text);
};

template <>
struct wire_traits<compass::serve::messages::address_note_request> {
    static constexpr bool is_generated = true;

    static std::string write_json(const compass::serve::messages::address_note_request& v);

    static compass::serve::messages::address_note_request read_json(std::string_view text);

    static compass::serve::messages::address_note_request read_body_json(std::string_view text);

    static std::string body_json_schema();
};

}

#endif

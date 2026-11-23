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
 * Template: masd_cpp_protocol_tests.cpp.mustache
 * To modify, update the template and regenerate.
 */
#include "compass.serve/messages/testing_queries_protocol.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "compass.db/codec/json_schema.hpp"
#include "compass.db/codec/member_count.hpp"
#include "compass.db/codec/sample.hpp"
#include "compass.db/codec/schema_check.hpp"

static_assert(compass::db::codec::names_every_member<compass::serve::messages::sprint_state_request>(),
    "sprint_state_request has a member its wire shape does not name");

TEST_CASE("sprint_state_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::sprint_state_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::sprint_state_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::sprint_state_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::sprint_state_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("sprint_state_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::sprint_state_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::sprint_state_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::sprint_state_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::sprint_state_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::sprint_state_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::what_needs_testing_request>(),
    "what_needs_testing_request has a member its wire shape does not name");

TEST_CASE("what_needs_testing_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::what_needs_testing_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::what_needs_testing_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::what_needs_testing_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::what_needs_testing_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("what_needs_testing_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::what_needs_testing_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::what_needs_testing_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::what_needs_testing_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::what_needs_testing_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::what_needs_testing_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::sprint_summary>(),
    "sprint_summary has a member its wire shape does not name");

TEST_CASE("sprint_summary reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::sprint_summary>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::sprint_summary>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::sprint_summary>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::sprint_summary empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("sprint_summary's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::sprint_summary>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::sprint_summary>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::sprint_summary>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::sprint_summary>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::sprint_summary>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::work_item>(),
    "work_item has a member its wire shape does not name");

TEST_CASE("work_item reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::work_item>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::work_item>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::work_item>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::work_item empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("work_item's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::work_item>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::work_item>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::work_item>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::work_item>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::work_item>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::story_state>(),
    "story_state has a member its wire shape does not name");

TEST_CASE("story_state reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::story_state>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::story_state>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::story_state>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::story_state empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("story_state's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::story_state>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::story_state>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::story_state>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::story_state>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::story_state>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::sprint_state_response>(),
    "sprint_state_response has a member its wire shape does not name");

TEST_CASE("sprint_state_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::sprint_state_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::sprint_state_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::sprint_state_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::sprint_state_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("sprint_state_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::sprint_state_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::sprint_state_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::sprint_state_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::sprint_state_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::sprint_state_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::run_summary>(),
    "run_summary has a member its wire shape does not name");

TEST_CASE("run_summary reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::run_summary>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::run_summary>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::run_summary>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::run_summary empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("run_summary's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::run_summary>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::run_summary>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::run_summary>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::run_summary>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::run_summary>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::scenario_status>(),
    "scenario_status has a member its wire shape does not name");

TEST_CASE("scenario_status reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::scenario_status>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::scenario_status>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::scenario_status>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::scenario_status empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("scenario_status's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::scenario_status>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::scenario_status>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::scenario_status>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::scenario_status>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::scenario_status>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::story_testing>(),
    "story_testing has a member its wire shape does not name");

TEST_CASE("story_testing reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::story_testing>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::story_testing>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::story_testing>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::story_testing empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("story_testing's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::story_testing>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::story_testing>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::story_testing>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::story_testing>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::story_testing>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::testing_response>(),
    "testing_response has a member its wire shape does not name");

TEST_CASE("testing_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::testing_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::testing_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::testing_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::testing_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("testing_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::testing_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::testing_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::testing_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::testing_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::testing_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::get_run_request>(),
    "get_run_request has a member its wire shape does not name");

TEST_CASE("get_run_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::get_run_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::get_run_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::get_run_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::get_run_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("get_run_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::get_run_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::get_run_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::get_run_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::get_run_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::get_run_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::run_finish>(),
    "run_finish has a member its wire shape does not name");

TEST_CASE("run_finish reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::run_finish>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::run_finish>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::run_finish>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::run_finish empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("run_finish's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::run_finish>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::run_finish>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::run_finish>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::run_finish>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::run_finish>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::step_result>(),
    "step_result has a member its wire shape does not name");

TEST_CASE("step_result reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::step_result>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::step_result>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::step_result>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::step_result empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("step_result's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::step_result>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::step_result>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::step_result>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::step_result>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::step_result>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::get_run_response>(),
    "get_run_response has a member its wire shape does not name");

TEST_CASE("get_run_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::get_run_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::get_run_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::get_run_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::get_run_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("get_run_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::get_run_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::get_run_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::get_run_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::get_run_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::get_run_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::list_notes_request>(),
    "list_notes_request has a member its wire shape does not name");

TEST_CASE("list_notes_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::list_notes_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::list_notes_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::list_notes_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::list_notes_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("list_notes_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::list_notes_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::list_notes_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::list_notes_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::list_notes_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::list_notes_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::note_address>(),
    "note_address has a member its wire shape does not name");

TEST_CASE("note_address reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::note_address>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::note_address>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::note_address>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::note_address empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("note_address's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::note_address>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::note_address>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::note_address>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::note_address>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::note_address>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::campaign_note>(),
    "campaign_note has a member its wire shape does not name");

TEST_CASE("campaign_note reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::campaign_note>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::campaign_note>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::campaign_note>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::campaign_note empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("campaign_note's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::campaign_note>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::campaign_note>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::campaign_note>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::campaign_note>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::campaign_note>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::list_notes_response>(),
    "list_notes_response has a member its wire shape does not name");

TEST_CASE("list_notes_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::list_notes_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::list_notes_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::list_notes_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::list_notes_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("list_notes_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::list_notes_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::list_notes_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::list_notes_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::list_notes_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::list_notes_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

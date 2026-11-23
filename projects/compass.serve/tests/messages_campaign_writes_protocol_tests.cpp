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
#include "compass.serve/messages/campaign_writes_protocol.hpp"

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

static_assert(compass::db::codec::names_every_member<compass::serve::messages::start_run_request>(),
    "start_run_request has a member its wire shape does not name");

TEST_CASE("start_run_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::start_run_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::start_run_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::start_run_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::start_run_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("start_run_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::start_run_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::start_run_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::start_run_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::start_run_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::start_run_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

TEST_CASE("start_run_request's route reads its body fields from a body its body "
    "schema allows, and no other member", "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::start_run_request>;
    const auto schema = traits::body_json_schema();
    const auto v = compass::db::codec::sample<compass::serve::messages::start_run_request>(0);
    const auto full = traits::write_json(v);
    const std::vector<std::string_view> body_names{ "scenario_id", };
    const auto body = compass::db::codec::only_members(full, body_names);
    INFO(body);
    CHECK(compass::db::codec::schema_violation(schema, body).value_or("") == "");
    const auto read = traits::read_body_json(body);
    CHECK(read.scenario_id == v.scenario_id);
    auto with_environment = body_names;
    with_environment.push_back("environment");
    CHECK_THROWS_AS(traits::read_body_json(compass::db::codec::only_members(full, with_environment)),
        std::invalid_argument);
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::run_step>(),
    "run_step has a member its wire shape does not name");

TEST_CASE("run_step reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::run_step>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::run_step>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::run_step>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::run_step empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("run_step's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::run_step>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::run_step>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::run_step>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::run_step>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::run_step>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::start_run_response>(),
    "start_run_response has a member its wire shape does not name");

TEST_CASE("start_run_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::start_run_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::start_run_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::start_run_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::start_run_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("start_run_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::start_run_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::start_run_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::start_run_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::start_run_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::start_run_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::step_result_request>(),
    "step_result_request has a member its wire shape does not name");

TEST_CASE("step_result_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::step_result_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::step_result_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::step_result_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::step_result_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("step_result_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::step_result_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::step_result_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::step_result_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::step_result_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::step_result_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

TEST_CASE("step_result_request's route reads its body fields from a body its body "
    "schema allows, and no other member", "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::step_result_request>;
    const auto schema = traits::body_json_schema();
    const auto v = compass::db::codec::sample<compass::serve::messages::step_result_request>(0);
    const auto full = traits::write_json(v);
    const std::vector<std::string_view> body_names{ "outcome", "notes", };
    const auto body = compass::db::codec::only_members(full, body_names);
    INFO(body);
    CHECK(compass::db::codec::schema_violation(schema, body).value_or("") == "");
    const auto read = traits::read_body_json(body);
    CHECK(read.outcome == v.outcome);
    CHECK(read.notes == v.notes);
    auto with_environment = body_names;
    with_environment.push_back("environment");
    CHECK_THROWS_AS(traits::read_body_json(compass::db::codec::only_members(full, with_environment)),
        std::invalid_argument);
    auto with_run = body_names;
    with_run.push_back("run");
    CHECK_THROWS_AS(traits::read_body_json(compass::db::codec::only_members(full, with_run)),
        std::invalid_argument);
    auto with_step_ord = body_names;
    with_step_ord.push_back("step_ord");
    CHECK_THROWS_AS(traits::read_body_json(compass::db::codec::only_members(full, with_step_ord)),
        std::invalid_argument);
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::finish_run_request>(),
    "finish_run_request has a member its wire shape does not name");

TEST_CASE("finish_run_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::finish_run_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::finish_run_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::finish_run_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::finish_run_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("finish_run_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::finish_run_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::finish_run_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::finish_run_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::finish_run_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::finish_run_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::finish_run_response>(),
    "finish_run_response has a member its wire shape does not name");

TEST_CASE("finish_run_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::finish_run_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::finish_run_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::finish_run_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::finish_run_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("finish_run_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::finish_run_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::finish_run_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::finish_run_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::finish_run_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::finish_run_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::attach_request>(),
    "attach_request has a member its wire shape does not name");

TEST_CASE("attach_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::attach_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::attach_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::attach_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::attach_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("attach_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::attach_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::attach_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::attach_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::attach_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::attach_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

TEST_CASE("attach_request's route reads its body fields from a body its body "
    "schema allows, and no other member", "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::attach_request>;
    const auto schema = traits::body_json_schema();
    const auto v = compass::db::codec::sample<compass::serve::messages::attach_request>(0);
    const auto full = traits::write_json(v);
    const std::vector<std::string_view> body_names{ "run", "note", "step_ord", "media_type", "bytes", "caption", };
    const auto body = compass::db::codec::only_members(full, body_names);
    INFO(body);
    CHECK(compass::db::codec::schema_violation(schema, body).value_or("") == "");
    const auto read = traits::read_body_json(body);
    CHECK(read.run == v.run);
    CHECK(read.note == v.note);
    CHECK(read.step_ord == v.step_ord);
    CHECK(read.media_type == v.media_type);
    CHECK(read.bytes == v.bytes);
    CHECK(read.caption == v.caption);
    auto with_environment = body_names;
    with_environment.push_back("environment");
    CHECK_THROWS_AS(traits::read_body_json(compass::db::codec::only_members(full, with_environment)),
        std::invalid_argument);
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::attach_response>(),
    "attach_response has a member its wire shape does not name");

TEST_CASE("attach_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::attach_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::attach_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::attach_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::attach_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("attach_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::attach_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::attach_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::attach_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::attach_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::attach_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::add_note_request>(),
    "add_note_request has a member its wire shape does not name");

TEST_CASE("add_note_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::add_note_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::add_note_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::add_note_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::add_note_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("add_note_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::add_note_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::add_note_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::add_note_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::add_note_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::add_note_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

TEST_CASE("add_note_request's route reads its body fields from a body its body "
    "schema allows, and no other member", "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::add_note_request>;
    const auto schema = traits::body_json_schema();
    const auto v = compass::db::codec::sample<compass::serve::messages::add_note_request>(0);
    const auto full = traits::write_json(v);
    const std::vector<std::string_view> body_names{ "scenario_id", "step_ord", "kind", "text", };
    const auto body = compass::db::codec::only_members(full, body_names);
    INFO(body);
    CHECK(compass::db::codec::schema_violation(schema, body).value_or("") == "");
    const auto read = traits::read_body_json(body);
    CHECK(read.scenario_id == v.scenario_id);
    CHECK(read.step_ord == v.step_ord);
    CHECK(read.kind == v.kind);
    CHECK(read.text == v.text);
    auto with_environment = body_names;
    with_environment.push_back("environment");
    CHECK_THROWS_AS(traits::read_body_json(compass::db::codec::only_members(full, with_environment)),
        std::invalid_argument);
    auto with_campaign = body_names;
    with_campaign.push_back("campaign");
    CHECK_THROWS_AS(traits::read_body_json(compass::db::codec::only_members(full, with_campaign)),
        std::invalid_argument);
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::add_note_response>(),
    "add_note_response has a member its wire shape does not name");

TEST_CASE("add_note_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::add_note_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::add_note_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::add_note_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::add_note_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("add_note_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::add_note_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::add_note_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::add_note_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::add_note_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::add_note_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::address_note_request>(),
    "address_note_request has a member its wire shape does not name");

TEST_CASE("address_note_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::address_note_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::address_note_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::address_note_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::address_note_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("address_note_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::address_note_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::address_note_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::address_note_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::address_note_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::address_note_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

TEST_CASE("address_note_request's route reads its body fields from a body its body "
    "schema allows, and no other member", "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::address_note_request>;
    const auto schema = traits::body_json_schema();
    const auto v = compass::db::codec::sample<compass::serve::messages::address_note_request>(0);
    const auto full = traits::write_json(v);
    const std::vector<std::string_view> body_names{ "reply", };
    const auto body = compass::db::codec::only_members(full, body_names);
    INFO(body);
    CHECK(compass::db::codec::schema_violation(schema, body).value_or("") == "");
    const auto read = traits::read_body_json(body);
    CHECK(read.reply == v.reply);
    auto with_environment = body_names;
    with_environment.push_back("environment");
    CHECK_THROWS_AS(traits::read_body_json(compass::db::codec::only_members(full, with_environment)),
        std::invalid_argument);
    auto with_note = body_names;
    with_note.push_back("note");
    CHECK_THROWS_AS(traits::read_body_json(compass::db::codec::only_members(full, with_note)),
        std::invalid_argument);
}

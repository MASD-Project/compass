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
#include <string>
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
}

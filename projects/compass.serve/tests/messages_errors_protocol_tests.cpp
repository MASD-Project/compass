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
#include "compass.serve/messages/errors_protocol.hpp"

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

static_assert(compass::db::codec::names_every_member<compass::serve::messages::error_response>(),
    "error_response has a member its wire shape does not name");

TEST_CASE("error_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::error_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::error_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::error_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::error_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("error_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::error_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::error_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::error_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::error_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::error_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

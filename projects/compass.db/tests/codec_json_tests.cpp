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
#include <chrono>
#include <stdexcept>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.db/codec/json_schema.hpp"
#include "compass.db/codec/schema_check.hpp"
#include "compass.db/types/note_row_codec.hpp"
#include "compass.db/types/placement_row_codec.hpp"

namespace {

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::MessageMatches;
using compass::db::types::note_row;
using compass::db::types::note_state;
using compass::db::types::placement;
using note_traits = compass::db::codec::wire_traits<note_row>;
using placement_traits = compass::db::codec::wire_traits<placement>;

/**
 * @brief A note as the writer writes it, with every member stated.
 */
std::string note_json() {
    note_row n;
    n.campaign = 7;
    n.kind = "remark";
    n.text = "the button is grey";
    n.author = "tester";
    n.state = note_state::addressed;
    n.addressed_at = std::chrono::sys_days{std::chrono::year{2026} / 11 / 23} +
        std::chrono::microseconds{1500};
    return note_traits::write_json(n);
}

/**
 * @brief @p text with its first @p from replaced by @p to.
 */
std::string replaced(std::string text, std::string_view from,
    std::string_view to) {
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

}

TEST_CASE("a row's JSON states its members by name and its enumerations "
    "and instants as text", "[codec][json]") {
    const auto text = note_json();
    CHECK_THAT(text, ContainsSubstring("\"campaign\":7"));
    CHECK_THAT(text, ContainsSubstring("\"state\":\"addressed\""));
    CHECK_THAT(text, ContainsSubstring(
        "\"addressed_at\":\"2026-11-23T00:00:00.001500Z\""));
    CHECK_THAT(text, ContainsSubstring("\"reply\":null"));
}

TEST_CASE("a reader refuses a member the row does not state, by name",
    "[codec][json]") {
    const auto text = replaced(note_json(), "{", "{\"colour\":\"grey\",");
    CHECK_THROWS_MATCHES(note_traits::read_json(text), std::invalid_argument,
        MessageMatches(ContainsSubstring("Unknown member: colour")));
}

TEST_CASE("a reader refuses a document that leaves out a required member, "
    "by name", "[codec][json]") {
    const auto text = replaced(note_json(), "\"author\":\"tester\",", "");
    CHECK_THROWS_MATCHES(note_traits::read_json(text), std::invalid_argument,
        MessageMatches(ContainsSubstring("Missing member: author")));
}

TEST_CASE("a reader takes an optional member left out as empty",
    "[codec][json]") {
    const auto text = replaced(note_json(), "\"reply\":null,", "");
    CHECK_FALSE(note_traits::read_json(text).reply.has_value());
}

TEST_CASE("a reader refuses a value of the wrong type, naming the member",
    "[codec][json]") {
    const auto number = replaced(note_json(), "\"campaign\":7",
        "\"campaign\":\"7\"");
    CHECK_THROWS_MATCHES(note_traits::read_json(number),
        std::invalid_argument,
        MessageMatches(ContainsSubstring("In member campaign: Not an integer")));
    const auto text = replaced(note_json(), "\"author\":\"tester\"",
        "\"author\":3");
    CHECK_THROWS_MATCHES(note_traits::read_json(text), std::invalid_argument,
        MessageMatches(ContainsSubstring("In member author: Not a string")));
}

TEST_CASE("a reader refuses an enumeration's text it does not list and an "
    "instant that is not one", "[codec][json]") {
    const auto state = replaced(note_json(), "\"addressed\"", "\"closed\"");
    CHECK_THROWS_MATCHES(note_traits::read_json(state), std::invalid_argument,
        MessageMatches(ContainsSubstring("In member state: Not a note_state: "
            "'closed' is not one of open, addressed")));
    const auto instant = replaced(note_json(), "2026-11-23T00:00:00.001500Z",
        "2026-11-23 00:00:00");
    CHECK_THROWS_MATCHES(note_traits::read_json(instant),
        std::invalid_argument,
        MessageMatches(ContainsSubstring("In member addressed_at: Not an "
            "instant")));
}

TEST_CASE("a blob ID travels as 40 hexadecimal digits", "[codec][json]") {
    placement p;
    p.path = "doc/a.org";
    p.hash.bytes[0] = 0xab;
    p.hash.bytes[19] = 0x01;
    const auto text = placement_traits::write_json(p);
    CHECK_THAT(text, ContainsSubstring(
        "\"hash\":\"ab00000000000000000000000000000000000001\""));
    CHECK(placement_traits::read_json(text) == p);
    CHECK_THROWS_MATCHES(placement_traits::read_json(
        replaced(text, "ab000", "zz000")), std::invalid_argument,
        MessageMatches(ContainsSubstring("In member hash: Not a blob ID")));
}

TEST_CASE("a row's schema requires its members, lists its enumeration and "
    "allows no other member", "[codec][schema]") {
    const auto schema = compass::db::codec::json_schema<note_row>();
    CHECK(compass::db::codec::json_schema_detail::definition_name<note_row>() ==
        "db.types.note_row");
    CHECK_THAT(schema, ContainsSubstring(
        "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\","
        "\"type\":\"object\""));
    CHECK_THAT(schema, ContainsSubstring("\"enum\":[\"open\",\"addressed\"]"));
    CHECK_THAT(schema, ContainsSubstring("\"additionalProperties\":false"));

    using compass::db::codec::schema_violation;
    CHECK_FALSE(schema_violation(schema, note_json()).has_value());
    CHECK(schema_violation(schema, replaced(note_json(),
        "\"author\":\"tester\",", "")) ==
        "$: the required member author is missing");
    CHECK(schema_violation(schema, replaced(note_json(), "{",
        "{\"colour\":\"grey\",")) == "$.colour: the schema states no such member");
    CHECK(schema_violation(schema, replaced(note_json(), "\"campaign\":7",
        "\"campaign\":\"7\"")) == "$.campaign: not of type integer");
    CHECK(schema_violation(schema, replaced(note_json(), "\"addressed\"",
        "\"closed\"")) ==
        "$.state: closed is not one of the values the schema lists");
    CHECK(schema_violation(schema, replaced(note_json(),
        "\"author\":\"tester\"", "\"author\":null")) ==
        "$.author: not of type string");
}

TEST_CASE("an instant's schema states the one spelling its reader takes, "
    "and a year of more than four digits has none", "[codec][schema]") {
    using instant = std::chrono::sys_time<std::chrono::microseconds>;
    using form = compass::db::codec::text_form<instant>;
    const auto schema = compass::db::codec::json_schema<note_row>();
    CHECK_THAT(schema, ContainsSubstring("\"format\":\"date-time\""));
    using compass::db::codec::schema_violation;
    CHECK(schema_violation(schema, replaced(note_json(), "2026-11-23T00:00:00.001500Z",
        "2026-11-23T10:00:00+01:00")).has_value());
    CHECK_FALSE(schema_violation(schema, replaced(note_json(),
        "2026-11-23T00:00:00.001500Z", "2026-11-23T10:00:00Z"))
        .has_value());
    using namespace std::chrono;
    CHECK_THROWS_AS(form::to_text(sys_days{year{12000} / 1 / 1}),
        std::invalid_argument);
    CHECK_THROWS_AS(form::to_text(sys_days{year{-1} / 1 / 1}),
        std::invalid_argument);
    CHECK(form::to_text(sys_days{year{9999} / 12 / 31}) ==
        "9999-12-31T00:00:00.000000Z");
}

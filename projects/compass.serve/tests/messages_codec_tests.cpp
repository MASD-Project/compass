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
#include <stdexcept>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.db/codec/json_schema.hpp"
#include "compass.db/codec/schema_check.hpp"
#include "compass.serve/messages/campaign_writes_protocol.hpp"
#include "compass.serve/messages/testing_queries_protocol.hpp"

namespace {

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::MessageMatches;
namespace messages = compass::serve::messages;
using attach_traits =
    compass::db::codec::wire_traits<messages::attach_request>;
using step_traits =
    compass::db::codec::wire_traits<messages::step_result_request>;

/**
 * @brief A screenshot of three bytes, one of them not text.
 */
messages::attach_request attach() {
    messages::attach_request a;
    a.environment = "main";
    a.run = 4;
    a.media_type = "image/png";
    a.bytes.bytes = std::string("\x89PN", 3);
    return a;
}

}

TEST_CASE("bytes travel as base64 and read back as the same bytes",
    "[codec][protocol]") {
    const auto text = attach_traits::write_json(attach());
    CHECK_THAT(text, ContainsSubstring("\"bytes\":\"iVBO\""));
    CHECK(attach_traits::read_json(text) == attach());
}

TEST_CASE("a reader refuses text that is not base64, naming the member",
    "[codec][protocol]") {
    auto text = attach_traits::write_json(attach());
    text.replace(text.find("iVBO"), 4, "iV?O");
    CHECK_THROWS_MATCHES(attach_traits::read_json(text),
        std::invalid_argument,
        MessageMatches(ContainsSubstring("In member bytes: Not base64")));
    auto short_text = attach_traits::write_json(attach());
    short_text.replace(short_text.find("iVBO"), 4, "iVB");
    CHECK_THROWS_MATCHES(attach_traits::read_json(short_text),
        std::invalid_argument,
        MessageMatches(ContainsSubstring("multiple of four")));
}

TEST_CASE("a request refuses a member the model does not state, such as "
    "the tester the gateway names", "[codec][protocol]") {
    const auto text = std::string("{\"environment\":\"main\",\"run\":1,"
        "\"step_ord\":2,\"outcome\":\"pass\",\"notes\":null,"
        "\"tester\":\"marco\"}");
    CHECK_THROWS_MATCHES(step_traits::read_json(text), std::invalid_argument,
        MessageMatches(ContainsSubstring("Unknown member: tester")));
    const auto schema = compass::db::codec::json_schema<
        messages::step_result_request>();
    CHECK(compass::db::codec::schema_violation(schema, text) ==
        "$.tester: the schema states no such member");
}

TEST_CASE("a request refuses an outcome its enumeration does not list and "
    "a required member left out", "[codec][protocol]") {
    const auto outcome = std::string("{\"environment\":\"main\",\"run\":1,"
        "\"step_ord\":2,\"outcome\":\"passed\"}");
    CHECK_THROWS_MATCHES(step_traits::read_json(outcome),
        std::invalid_argument,
        MessageMatches(ContainsSubstring("In member outcome: Not a "
            "step_outcome: 'passed' is not one of pass, fail, blocked, "
            "skipped")));
    const auto missing = std::string("{\"environment\":\"main\",\"run\":1,"
        "\"outcome\":\"pass\"}");
    CHECK_THROWS_MATCHES(step_traits::read_json(missing),
        std::invalid_argument,
        MessageMatches(ContainsSubstring("Missing member: step_ord")));
}

TEST_CASE("the two headers that declare step_outcome can be included "
    "together and agree", "[codec][protocol]") {
    messages::step_result r;
    r.outcome = messages::step_outcome::skipped;
    CHECK(compass::db::codec::text_form<messages::step_outcome>::to_text(
              *r.outcome) == "skipped");
}

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
#include <tuple>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.db/codec/json_document.hpp"
#include "compass.db/codec/json_schema.hpp"
#include "compass.db/codec/schema_check.hpp"
#include "compass.serve/messages/campaign_writes_protocol.hpp"
#include "compass.serve/messages/document_queries_protocol.hpp"
#include "compass.serve/messages/testing_queries_protocol.hpp"

namespace {

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::MessageMatches;
namespace codec = compass::db::codec;
namespace messages = compass::serve::messages;
using attach_traits = codec::wire_traits<messages::attach_request>;
using step_traits = codec::wire_traits<messages::step_result_request>;
using document_traits = codec::wire_traits<messages::get_document_request>;

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

/**
 * @brief The names @p schema's root requires, in order.
 */
std::vector<std::string> required_of(const std::string& schema) {
    const auto d = codec::json_document::parse(schema);
    std::vector<std::string> out;
    for (const auto& name : d->root().member("required").elements())
        out.push_back(name.text_or());
    return out;
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

TEST_CASE("bytes have one base64 text, which the schema states",
    "[codec][protocol][schema]") {
    CHECK(codec::text_form<codec::base64_bytes>::from_text("aGk=").bytes ==
        "hi");
    CHECK_THROWS_MATCHES(codec::text_form<codec::base64_bytes>::from_text(
        "aGl="), std::invalid_argument,
        MessageMatches(ContainsSubstring("not zero")));
    CHECK_THROWS_AS(codec::text_form<codec::base64_bytes>::from_text("aR=="),
        std::invalid_argument);
    const auto schema = codec::json_schema<messages::attach_request>();
    CHECK_THAT(schema, ContainsSubstring("\"contentEncoding\":\"base64\""));
    auto text = attach_traits::write_json(attach());
    CHECK(codec::schema_violation(schema, text).value_or("") == "");
    text.replace(text.find("iVBO"), 4, "aGl=");
    CHECK(codec::schema_violation(schema, text).has_value());
}

TEST_CASE("a narrow integer's schema states the range its reader takes",
    "[codec][protocol][schema]") {
    const auto schema = codec::json_schema<messages::step_result_request>();
    CHECK_THAT(schema, ContainsSubstring(
        "\"step_ord\":{\"type\":\"integer\",\"minimum\":-2147483648,"
        "\"maximum\":2147483647}"));
    const auto text = std::string("{\"environment\":\"main\",\"run\":1,"
        "\"step_ord\":4294967296}");
    CHECK_THROWS_AS(step_traits::read_json(text), std::invalid_argument);
    CHECK(codec::schema_violation(schema, text) ==
        "$.step_ord: 4294967296 is above the maximum");
}

TEST_CASE("a request's MCP arguments are every field, and the binding rules "
    "say which may be left out", "[codec][protocol][schema]") {
    const auto schema = codec::json_schema<messages::search_request>();
    CHECK(required_of(schema) == std::vector<std::string>{"environment", "q"});
    CHECK_THAT(schema, ContainsSubstring(
        "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\","
        "\"type\":\"object\",\"properties\":{"));
    const auto read = codec::wire_traits<messages::search_request>::read_json(
        "{\"environment\":\"main\",\"q\":\"codec\"}");
    CHECK(read.limit == 20);
    CHECK(read.offset == 0);
    CHECK(read.filter == messages::document_filter{});
    const auto facets = codec::json_schema<messages::facets_request>();
    CHECK(required_of(facets) == std::vector<std::string>{"environment"});
}

TEST_CASE("a path field is always given, though it has a default",
    "[codec][protocol]") {
    const auto missing = std::string("{\"environment\":\"main\",\"run\":1,"
        "\"outcome\":\"pass\"}");
    CHECK_THROWS_MATCHES(step_traits::read_json(missing),
        std::invalid_argument,
        MessageMatches(ContainsSubstring("Missing member: step_ord")));
    CHECK(required_of(codec::json_schema<messages::step_result_request>()) ==
        std::vector<std::string>{"environment", "run", "step_ord"});
}

TEST_CASE("a route reads exactly its body fields from the body, and a "
    "defaulted one may be left out", "[codec][protocol][schema]") {
    const auto schema = step_traits::body_json_schema();
    CHECK(required_of(schema).empty());
    const auto stated = step_traits::read_body_json(
        "{\"outcome\":\"fail\",\"notes\":null}");
    CHECK(stated.outcome == messages::step_outcome::fail);
    CHECK_FALSE(stated.notes.has_value());
    CHECK(step_traits::read_body_json("{}").outcome ==
        messages::step_outcome::pass);
    CHECK(codec::schema_violation(schema, "{}").value_or("") == "");
    const auto path_in_body = std::string("{\"environment\":\"main\","
        "\"outcome\":\"pass\"}");
    CHECK_THROWS_MATCHES(step_traits::read_body_json(path_in_body),
        std::invalid_argument,
        MessageMatches(ContainsSubstring("Unknown member: environment")));
    CHECK(codec::schema_violation(schema, path_in_body) ==
        "$.environment: the schema states no such member");
}

TEST_CASE("a request refuses a member the model does not state, such as "
    "the tester the gateway names", "[codec][protocol]") {
    const auto text = std::string("{\"environment\":\"main\",\"run\":1,"
        "\"step_ord\":2,\"outcome\":\"pass\",\"notes\":null,"
        "\"tester\":\"marco\"}");
    CHECK_THROWS_MATCHES(step_traits::read_json(text), std::invalid_argument,
        MessageMatches(ContainsSubstring("Unknown member: tester")));
    const auto schema = codec::json_schema<messages::step_result_request>();
    CHECK(codec::schema_violation(schema, text) ==
        "$.tester: the schema states no such member");
}

TEST_CASE("a request refuses a member stated twice", "[codec][protocol]") {
    const auto text = std::string("{\"environment\":\"main\","
        "\"environment\":\"other\",\"run\":1,\"step_ord\":2}");
    CHECK_THROWS_MATCHES(step_traits::read_json(text), std::invalid_argument,
        MessageMatches(ContainsSubstring("Duplicate member: environment")));
}

TEST_CASE("a request refuses a value its enumeration does not list, and "
    "its schema lists the values", "[codec][protocol][schema]") {
    const auto outcome = std::string("{\"environment\":\"main\",\"run\":1,"
        "\"step_ord\":2,\"outcome\":\"passed\"}");
    CHECK_THROWS_MATCHES(step_traits::read_json(outcome),
        std::invalid_argument,
        MessageMatches(ContainsSubstring("In member outcome: Not a "
            "step_outcome: 'passed' is not one of pass, fail, blocked, "
            "skipped")));
    const auto pdf = std::string("{\"environment\":\"main\",\"id\":\"x\","
        "\"format\":\"pdf\"}");
    CHECK_THROWS_MATCHES(document_traits::read_json(pdf),
        std::invalid_argument,
        MessageMatches(ContainsSubstring("'pdf' is not one of html, org")));
    const auto schema = codec::json_schema<messages::get_document_request>();
    CHECK_THAT(schema, ContainsSubstring(
        "\"format\":{\"type\":\"string\",\"enum\":[\"html\",\"org\"]}"));
    CHECK(codec::schema_violation(schema, pdf).has_value());
}

TEST_CASE("a refusal of text that is not JSON says where, not what",
    "[codec][protocol]") {
    const auto text = "{\"environment\":\"" + std::string(1000, 'x');
    try {
        std::ignore = step_traits::read_json(text);
        FAIL("the reader took text that is not JSON");
    } catch (const std::invalid_argument& e) {
        const std::string what = e.what();
        CHECK_THAT(what, ContainsSubstring("Not JSON: "));
        CHECK_THAT(what, ContainsSubstring(" at byte "));
        CHECK(what.size() < 200);
    }
}

TEST_CASE("the two headers that declare step_outcome can be included "
    "together and agree", "[codec][protocol]") {
    messages::step_result r;
    r.outcome = messages::step_outcome::skipped;
    CHECK(codec::text_form<messages::step_outcome>::to_text(*r.outcome) ==
        "skipped");
}

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
#include "compass.serve/messages/document_queries_protocol.hpp"

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

static_assert(compass::db::codec::names_every_member<compass::serve::messages::metadata_match>(),
    "metadata_match has a member its wire shape does not name");

TEST_CASE("metadata_match reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::metadata_match>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::metadata_match>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::metadata_match>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::metadata_match empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("metadata_match's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::metadata_match>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::metadata_match>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::metadata_match>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::metadata_match>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::metadata_match>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::document_filter>(),
    "document_filter has a member its wire shape does not name");

TEST_CASE("document_filter reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::document_filter>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::document_filter>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::document_filter>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::document_filter empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("document_filter's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::document_filter>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::document_filter>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::document_filter>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::document_filter>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::document_filter>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::search_request>(),
    "search_request has a member its wire shape does not name");

TEST_CASE("search_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::search_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::search_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::search_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::search_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("search_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::search_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::search_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::search_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::search_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::search_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::search_hit>(),
    "search_hit has a member its wire shape does not name");

TEST_CASE("search_hit reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::search_hit>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::search_hit>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::search_hit>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::search_hit empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("search_hit's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::search_hit>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::search_hit>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::search_hit>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::search_hit>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::search_hit>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::search_response>(),
    "search_response has a member its wire shape does not name");

TEST_CASE("search_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::search_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::search_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::search_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::search_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("search_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::search_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::search_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::search_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::search_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::search_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::facets_request>(),
    "facets_request has a member its wire shape does not name");

TEST_CASE("facets_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::facets_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::facets_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::facets_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::facets_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("facets_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::facets_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::facets_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::facets_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::facets_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::facets_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::facet_count>(),
    "facet_count has a member its wire shape does not name");

TEST_CASE("facet_count reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::facet_count>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::facet_count>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::facet_count>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::facet_count empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("facet_count's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::facet_count>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::facet_count>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::facet_count>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::facet_count>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::facet_count>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::named_facet>(),
    "named_facet has a member its wire shape does not name");

TEST_CASE("named_facet reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::named_facet>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::named_facet>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::named_facet>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::named_facet empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("named_facet's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::named_facet>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::named_facet>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::named_facet>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::named_facet>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::named_facet>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::facets_response>(),
    "facets_response has a member its wire shape does not name");

TEST_CASE("facets_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::facets_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::facets_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::facets_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::facets_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("facets_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::facets_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::facets_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::facets_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::facets_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::facets_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::document_summary>(),
    "document_summary has a member its wire shape does not name");

TEST_CASE("document_summary reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::document_summary>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::document_summary>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::document_summary>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::document_summary empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("document_summary's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::document_summary>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::document_summary>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::document_summary>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::document_summary>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::document_summary>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::list_documents_request>(),
    "list_documents_request has a member its wire shape does not name");

TEST_CASE("list_documents_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::list_documents_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::list_documents_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::list_documents_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::list_documents_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("list_documents_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::list_documents_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::list_documents_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::list_documents_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::list_documents_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::list_documents_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::list_documents_response>(),
    "list_documents_response has a member its wire shape does not name");

TEST_CASE("list_documents_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::list_documents_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::list_documents_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::list_documents_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::list_documents_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("list_documents_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::list_documents_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::list_documents_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::list_documents_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::list_documents_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::list_documents_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::get_document_request>(),
    "get_document_request has a member its wire shape does not name");

TEST_CASE("get_document_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::get_document_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::get_document_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::get_document_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::get_document_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("get_document_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::get_document_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::get_document_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::get_document_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::get_document_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::get_document_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::get_document_response>(),
    "get_document_response has a member its wire shape does not name");

TEST_CASE("get_document_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::get_document_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::get_document_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::get_document_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::get_document_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("get_document_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::get_document_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::get_document_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::get_document_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::get_document_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::get_document_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::backlinks_request>(),
    "backlinks_request has a member its wire shape does not name");

TEST_CASE("backlinks_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::backlinks_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::backlinks_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::backlinks_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::backlinks_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("backlinks_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::backlinks_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::backlinks_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::backlinks_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::backlinks_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::backlinks_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::backlink>(),
    "backlink has a member its wire shape does not name");

TEST_CASE("backlink reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::backlink>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::backlink>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::backlink>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::backlink empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("backlink's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::backlink>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::backlink>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::backlink>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::backlink>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::backlink>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::backlinks_response>(),
    "backlinks_response has a member its wire shape does not name");

TEST_CASE("backlinks_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::backlinks_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::backlinks_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::backlinks_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::backlinks_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("backlinks_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::backlinks_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::backlinks_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::backlinks_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::backlinks_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::backlinks_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::list_environments_request>(),
    "list_environments_request has a member its wire shape does not name");

TEST_CASE("list_environments_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::list_environments_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::list_environments_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::list_environments_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::list_environments_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("list_environments_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::list_environments_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::list_environments_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::list_environments_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::list_environments_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::list_environments_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::environment_summary>(),
    "environment_summary has a member its wire shape does not name");

TEST_CASE("environment_summary reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::environment_summary>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::environment_summary>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::environment_summary>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::environment_summary empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("environment_summary's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::environment_summary>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::environment_summary>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::environment_summary>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::environment_summary>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::environment_summary>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::list_environments_response>(),
    "list_environments_response has a member its wire shape does not name");

TEST_CASE("list_environments_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::list_environments_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::list_environments_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::list_environments_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::list_environments_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("list_environments_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::list_environments_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::list_environments_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::list_environments_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::list_environments_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::list_environments_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::version_request>(),
    "version_request has a member its wire shape does not name");

TEST_CASE("version_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::version_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::version_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::version_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::version_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("version_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::version_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::version_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::version_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::version_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::version_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::version_response>(),
    "version_response has a member its wire shape does not name");

TEST_CASE("version_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::version_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::version_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::version_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::version_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("version_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::version_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::version_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::version_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::version_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::version_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::sync_request>(),
    "sync_request has a member its wire shape does not name");

TEST_CASE("sync_request reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::sync_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::sync_request>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::sync_request>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::sync_request empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("sync_request's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::sync_request>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::sync_request>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::sync_request>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::sync_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::sync_request>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

static_assert(compass::db::codec::names_every_member<compass::serve::messages::sync_response>(),
    "sync_response has a member its wire shape does not name");

TEST_CASE("sync_response reads back the JSON it writes, which its JSON Schema "
    "allows", "[codec][roundtrip][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::sync_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::sync_response>();
    for (const auto which : {std::size_t(0), std::size_t(1)}) {
        const auto v = compass::db::codec::sample<compass::serve::messages::sync_response>(which);
        const auto text = traits::write_json(v);
        INFO(text);
        CHECK(traits::read_json(text) == v);
        CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
    }
    const compass::serve::messages::sync_response empty{};
    const auto text = traits::write_json(empty);
    INFO(text);
    CHECK(traits::read_json(text) == empty);
    CHECK(compass::db::codec::schema_violation(schema, text).value_or("") == "");
}

TEST_CASE("sync_response's reader and JSON Schema refuse the same members",
    "[codec][schema][protocol]") {
    using traits = compass::db::codec::wire_traits<compass::serve::messages::sync_response>;
    const auto schema = compass::db::codec::json_schema<compass::serve::messages::sync_response>();
    const auto text = traits::write_json(compass::db::codec::sample<compass::serve::messages::sync_response>(0));
    const auto added = compass::db::codec::with_unknown_member(text);
    CHECK_THROWS_AS(traits::read_json(added), std::invalid_argument);
    CHECK(compass::db::codec::schema_violation(schema, added).has_value());
    for (const auto name : compass::db::codec::required_members<compass::serve::messages::sync_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_THROWS_AS(traits::read_json(removed), std::invalid_argument);
        CHECK(compass::db::codec::schema_violation(schema, removed).has_value());
    }
    for (const auto name : compass::db::codec::absent_members<compass::serve::messages::sync_response>()) {
        INFO(name);
        const auto removed = compass::db::codec::without_member(text, name);
        CHECK_NOTHROW(traits::read_json(removed));
        CHECK(compass::db::codec::schema_violation(schema, removed).value_or("") == "");
    }
}

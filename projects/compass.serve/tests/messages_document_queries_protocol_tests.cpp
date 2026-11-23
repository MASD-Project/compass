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
#include <string>
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
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
    CHECK(traits::read_json(traits::write_json(empty)) == empty);
}

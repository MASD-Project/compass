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
#include "compass.codegen/ops/entity_steps.hpp"

#include <catch2/catch_test_macros.hpp>
#include "entity_fixture.hpp"

namespace {

using namespace compass;
using codegen::ops::values::items;
using codegen::ops::values::key_of;
using codegen::ops::values::member;

/*
 * Requests as derive_messages states them: a paged list, a scoped list,
 * a versions read with its verb stated, a key read, a put and a batch
 * delete, with a response and a declared message the operations skip.
 */
constexpr std::string_view currency = R"j({"entity_singular": "currency",
 "open_reads": " refdata.v1.currencies.list\trefdata.v1.currencies.get ",
 "messages": [
  {"name": "list_currencies_request", "derived": true,
   "subject": "refdata.v1.currencies.list",
   "response_type": "list_currencies_response",
   "fields": [{"name": "offset", "cpp_type": "std::uint32_t"},
              {"name": "order", "cpp_type": "ores::utility::domain::order"},
              {"name": "filter",
               "cpp_type": "std::optional<currencies_filter>"}]},
  {"name": "list_currencies_response", "derived": true, "fields": []},
  {"name": "list_by_party_id_request", "derived": true,
   "subject": "refdata.v1.currencies.list_by_party_id",
   "response_type": "list_currencies_response", "requires_session": "false",
   "fields": [{"name": "party_id",
               "cpp_type": "std::optional<boost::uuids::uuid>"},
              {"name": "scope", "cpp_type": "ores::utility::domain::scope"}]},
  {"name": "get_currency_versions_request", "derived": true,
   "subject": "refdata.v1.currencies_versions.list",
   "verb": "list_versions",
   "response_type": "get_currency_versions_response",
   "fields": [{"name": "valid_from",
               "cpp_type": "std::chrono::system_clock::time_point"}]},
  {"name": "get_currencies_request", "derived": true,
   "subject": "refdata.v1.currencies.get",
   "response_type": "get_currencies_response", "fields": []},
  {"name": "put_currency_request", "derived": true,
   "subject": "refdata.v1.currencies.put",
   "response_type": "put_currency_response",
   "fields": [{"name": "data", "cpp_type": "currency_write"}]},
  {"name": "delete_many_currencies_request", "derived": true,
   "subject": "refdata.v1.currencies.delete_many",
   "response_type": "delete_many_currencies_response", "fields": []},
  {"name": "lock_currency_request", "subject": "refdata.v1.currencies.lock",
   "fields": []}]})j";

/*
 * protocol_operations of the fixture, without fields, as ores.codegen
 * computes it.
 */
constexpr std::string_view expected[] = {
    R"j({"has_filter": true, "has_order": true, "has_scope": false, "is_guarded": false, "is_put_one": false, "is_write": false, "leading": "offset", "leading_is_optional": false, "leading_is_timestamp": false, "leading_is_uuid": false, "method": "list_currencies", "permission": "", "request": "list_currencies_request", "requires_session": true, "response": "list_currencies_response", "subject": "refdata.v1.currencies.list", "verb": "list"})j",
    R"j({"has_filter": false, "has_order": false, "has_scope": true, "is_guarded": true, "is_put_one": false, "is_write": false, "leading": "party_id", "leading_is_optional": true, "leading_is_timestamp": false, "leading_is_uuid": true, "method": "list_by_party_id", "permission": "read", "request": "list_by_party_id_request", "requires_session": false, "response": "list_currencies_response", "subject": "refdata.v1.currencies.list_by_party_id", "verb": "list_scoped"})j",
    R"j({"has_filter": false, "has_order": false, "has_scope": false, "is_guarded": true, "is_put_one": false, "is_write": false, "leading": "valid_from", "leading_is_optional": false, "leading_is_timestamp": true, "leading_is_uuid": false, "method": "get_currency_versions", "permission": "read", "request": "get_currency_versions_request", "requires_session": true, "response": "get_currency_versions_response", "subject": "refdata.v1.currencies_versions.list", "verb": "list_versions"})j",
    R"j({"has_filter": false, "has_order": false, "has_scope": false, "is_guarded": false, "is_put_one": false, "is_write": false, "leading": "", "leading_is_optional": false, "leading_is_timestamp": false, "leading_is_uuid": false, "method": "get_currencies", "permission": "", "request": "get_currencies_request", "requires_session": true, "response": "get_currencies_response", "subject": "refdata.v1.currencies.get", "verb": "get"})j",
    R"j({"has_filter": false, "has_order": false, "has_scope": false, "is_guarded": true, "is_put_one": true, "is_write": true, "leading": "data", "leading_is_optional": false, "leading_is_timestamp": false, "leading_is_uuid": false, "method": "put_currency", "permission": "write", "request": "put_currency_request", "requires_session": true, "response": "put_currency_response", "subject": "refdata.v1.currencies.put", "verb": "put"})j",
    R"j({"has_filter": false, "has_order": false, "has_scope": false, "is_guarded": true, "is_put_one": false, "is_write": true, "leading": "", "leading_is_optional": false, "leading_is_timestamp": false, "leading_is_uuid": false, "method": "delete_many_currencies", "permission": "delete", "request": "delete_many_currencies_request", "requires_session": true, "response": "delete_many_currencies_response", "subject": "refdata.v1.currencies.delete_many", "verb": "delete_many"})j"};

}

TEST_CASE("each derived request with a subject is one operation",
    "[codegen][entity][operations]") {
    codegen::tests::entity_fixture f(currency);
    codegen::ops::derive_operations(f.b, f.entity);
    const auto operations = items(f.tables,
        member(f.tables, f.entity, "operations"));
    REQUIRE(operations.size() == std::size(expected));
    for (std::size_t i = 0; i < operations.size(); ++i) {
        const auto want = model::io::read_json(f.b, expected[i]);
        for (const auto m : items(f.tables, want)) {
            const auto key = key_of(f.tables, m);
            INFO("operation " << i << " member " << key);
            CHECK(f.json(operations[i], key) == f.json(want, key));
        }
    }
    const auto messages = items(f.tables,
        member(f.tables, f.entity, "messages"));
    CHECK(f.json(operations[0], "fields") == f.json(messages[0], "fields"));
}

TEST_CASE("operations are grouped by verb", "[codegen][entity][operations]") {
    codegen::tests::entity_fixture f(currency);
    codegen::ops::derive_operations(f.b, f.entity);
    const auto methods = [&](std::string_view key) {
        std::vector<std::string> out;
        for (const auto o : items(f.tables, member(f.tables, f.entity, key)))
            out.emplace_back(codegen::ops::values::str(f.tables, o, "method"));
        return out;
    };
    CHECK(methods("list_scoped_operations") ==
        std::vector<std::string>{"list_by_party_id"});
    CHECK(methods("list_versions_operations") ==
        std::vector<std::string>{"get_currency_versions"});
    CHECK(methods("delete_many_operations") ==
        std::vector<std::string>{"delete_many_currencies"});
    CHECK(f.json("get_many_operations") == "<absent>");
}

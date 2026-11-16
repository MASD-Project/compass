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
using codegen::ops::values::member;

}

/*
 * ORE Studio's calendar_exception; expected values are the oracle's.
 */
TEST_CASE("the table display flags uuid, optional and bool columns",
    "[codegen][entity][repository]") {
    codegen::tests::entity_fixture f(R"j({
     "primary_key": {"column": "id", "is_uuid": true},
     "columns": [{"name": "is_business_day", "cpp_type": "bool"},
                 {"name": "description",
                  "cpp_type": "std::optional<std::string>"}],
     "cpp": {"iterator_var": "ce", "table_display": [
       {"column": "id"}, {"column": "is_business_day"},
       {"column": "description"}, {"column": "version"}]}})j");
    codegen::ops::derive_table_display(f.b, f.entity);
    const auto cpp = member(f.tables, f.entity, "cpp");
    const auto rows = items(f.tables, cpp, "table_display");
    CHECK(f.json(rows[0], "is_uuid") == "true");
    CHECK(f.json(rows[1], "is_bool") == "true");
    CHECK(f.json(rows[2], "is_optional") == "true");
    CHECK(f.json(rows[2], "is_bool") == "false");
    CHECK(f.json(rows[3], "is_uuid") == "false");
    CHECK(f.json(rows[3], "iter_var") == "\"ce\"");
    CHECK(f.json(cpp, "has_uuid_table_display") == "true");
    CHECK(f.json(cpp, "has_optional_table_display") == "true");
}

/*
 * ORE Studio's convention reaches its displayed columns through members.
 */
TEST_CASE("a grouped entity's table display names the member path",
    "[codegen][entity][repository]") {
    codegen::tests::entity_fixture f(R"j({
     "domain_group_fields": [{"name": "identity.kind_code",
       "cpp_type": "std::string"}, {"name": "payload.name",
       "cpp_type": "std::string"}],
     "cpp": {"table_display": [{"column": "kind_code"},
       {"column": "payload.name"}, {"column": "note"}]}})j");
    codegen::ops::derive_table_display(f.b, f.entity);
    const auto rows = items(f.tables, member(f.tables, f.entity, "cpp"),
        "table_display");
    CHECK(f.json(rows[0], "column") == "\"identity.kind_code\"");
    CHECK(f.json(rows[1], "column") == "\"payload.name\"");
    CHECK(f.json(rows[2], "column") == "\"note\"");
    CHECK(f.json(rows[0], "iter_var") == "\"e\"");
}

TEST_CASE("the repository section reaches the entity's top level",
    "[codegen][entity][repository]") {
    codegen::tests::entity_fixture f(R"j({"entity_singular_words": "x",
     "repository": {"entity_singular_words": "account contact information",
       "tenant_read_scope": "shared"}})j");
    codegen::ops::copy_repository_fields(f.b, f.entity);
    CHECK(f.json("entity_singular_words") ==
        "\"account contact information\"");
    CHECK(f.json("tenant_read_scope") == "\"shared\"");
}

/*
 * ORE Studio's subject_area: a shared read and a composite text key;
 * expected values are the oracle's.
 */
TEST_CASE("a shared composite-key entity derives its scope and defaults",
    "[codegen][entity][repository]") {
    codegen::tests::entity_fixture f(R"j({"has_tenant_id": true,
     "tenant_read_scope": "shared",
     "primary_key": {"column": "name", "type": "text", "columns": [
       {"column": "name"}, {"column": "domain_name"}]}})j");
    codegen::ops::derive_tenant_scope(f.b, f.entity);
    codegen::ops::derive_messaging_defaults(f.b, f.entity);
    CHECK(f.json("read_tenant_filtered") == "false");
    CHECK(f.json("system_tenant_visible") == "false");
    CHECK(f.json("tenant_where") == R"j("\"tenant_id\"_c == tid")j");
    CHECK(f.json("sys_decl") == "\"\"");
    CHECK(f.json("delete_request_id_field") == "\"names\"");
    CHECK(f.json("history_request_id_field") == "\"name\"");
    CHECK(f.json("history_request_extra_args") == R"j([
  {
    "name": "domain_name"
  }
])j");
    CHECK(f.json("delete_request_extra_args") == R"j([
  {
    "name": "domain_names"
  }
])j");
    CHECK(f.json("single_delete") == "false");
}

TEST_CASE("a system-visible uuid-key entity reads the system tenant too",
    "[codegen][entity][repository]") {
    codegen::tests::entity_fixture f(R"j({"has_tenant_id": true,
     "system_tenant_visible": true, "single_delete": true,
     "primary_key": {"column": "id", "type": "uuid"}})j");
    codegen::ops::derive_tenant_scope(f.b, f.entity);
    codegen::ops::derive_messaging_defaults(f.b, f.entity);
    CHECK(f.json("read_tenant_filtered") == "true");
    CHECK(f.json("tenant_where") ==
        R"j("(\"tenant_id\"_c == tid || \"tenant_id\"_c == sys)")j");
    CHECK(f.json("sys_decl") == R"j("\n    static const std::string sys(ores::database::service::tenant_context::system_tenant_id);")j");
    CHECK(f.json("delete_request_id_field") == "\"ids\"");
    CHECK(f.json("history_request_extra_args") == "[]");
    CHECK(f.json("single_delete") == "true");
}

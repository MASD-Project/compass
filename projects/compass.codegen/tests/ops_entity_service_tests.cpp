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

}

/*
 * Expected values are _service_include_gates' for the same entity.
 */
TEST_CASE("a derived protocol gates the service's includes on its operations",
    "[codegen][entity][service]") {
    codegen::tests::entity_fixture f(R"j({"component": "refdata",
        "entity_singular": "currency", "list_versions_operations": [{}],
        "put_operations": [{}],
        "write_fields": [{"name": "a"}, {"name": "b", "date_from_string": true}],
        "primary_key": {"columns": [{"is_uuid": false}], "is_compound": true},
        "foreign_keys": [{"list_by": true, "list_by_uuid": true}],
        "list_scoped_operations": [{"leading_is_uuid": false}]})j");
    codegen::ops::derive_service_gates(f.b, f.entity, {});
    CHECK(f.json("protocol_derived") == "true");
    CHECK(f.json("service_pages_versions") == "true");
    CHECK(f.json("service_uses_datetime") == "true");
    CHECK(f.json("service_parses_declared_uuid") == "false");
    CHECK(f.json("service_uses_uuid") == "false");
    CHECK(f.json("service_lists_by_as_of") == "false");
    CHECK(f.json("service_uses_uuid_io") == "true");
    CHECK(f.json("repository_uses_utility") == "true");
}

TEST_CASE("an owned protocol drops the gates its operations open",
    "[codegen][entity][service]") {
    codegen::tests::entity_fixture f(R"j({"component": "iam",
        "entity_plural": "tenants", "list_versions_operations": [{}],
        "put_operations": [{}],
        "write_fields": [{"timestamp_from_string": true}],
        "key_is_primary": false, "declared_key_is_uuid": true,
        "primary_key": {"has_timestamp_key": false, "columns": []},
        "foreign_keys": [{"list_by_as_of": true}],
        "list_scoped_operations": [{"leading_is_uuid": true}]})j");
    codegen::types::entity_site site;
    site.protocol_owned = true;
    codegen::ops::derive_service_gates(f.b, f.entity, site);
    CHECK(f.json("protocol_derived") == "false");
    CHECK(f.json("service_pages_versions") == "false");
    CHECK(f.json("service_uses_datetime") == "false");
    CHECK(f.json("service_parses_declared_uuid") == "true");
    CHECK(f.json("service_uses_uuid") == "true");
    CHECK(f.json("service_lists_by_as_of") == "true");
    CHECK(f.json("service_uses_uuid_io") == "true");
    CHECK(f.json("repository_uses_utility") == "false");
}

TEST_CASE("events are announced under the plural, or the singular plus s",
    "[codegen][entity][service]") {
    codegen::tests::entity_fixture f(R"j({"component": "refdata",
        "entity_singular": "currency"})j");
    codegen::ops::derive_events(f.b, f.entity);
    CHECK(f.json("event_prefix") == R"("refdata.v1.events.currencys")");
    CHECK(f.strings("events").size() == 3);
    const auto events = codegen::ops::values::items(f.tables,
        codegen::ops::values::member(f.tables, f.entity, "events"));
    CHECK(f.json(events[1], "action") == R"("updated")");
    CHECK(f.json(events[1], "subject") ==
        R"("refdata.v1.events.currencys.updated")");
}

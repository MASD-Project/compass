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

void derive(codegen::tests::entity_fixture& f) {
    codegen::ops::derive_list_requests(f.b, f.entity);
    codegen::ops::apply_stated_order(f.b, f.entity);
}

}

/*
 * ORE Studio's market_observation; expected values are the oracle's.
 */
TEST_CASE("a list_by foreign key derives its request and its own order",
    "[codegen][entity][order]") {
    codegen::tests::entity_fixture f(R"j({
     "entity_plural_short": "market_observations",
     "primary_key": {"column": "id", "type": "uuid",
       "columns": [{"column": "id"}]},
     "foreign_keys": [{"column": "series_id", "list_by": true,
       "list_by_order_by": "observation_datetime desc",
       "list_by_default_limit": 1000}, {"column": "source_id"}]})j");
    derive(f);
    CHECK(f.json("extra_list_requests") == R"j([
  {
    "count_service_method": "count_market_observations_by_series_id",
    "default_limit": 1000,
    "filter_column": "series_id",
    "name_suffix": "by_series_id",
    "nats_suffix": "list_by_series_id",
    "service_method": "list_market_observations_by_series_id"
  }
])j");
    const auto fks = items(f.tables, f.entity, "foreign_keys");
    CHECK(f.json(fks[0], "list_by_order_column") == "\"observation_datetime\"");
    CHECK(f.json(fks[0], "list_by_order_desc") == "true");
    CHECK(f.json(fks[0], "list_by_default_columns") ==
        R"j("{\"observation_datetime\"}")j");
    CHECK(f.json(fks[0], "list_by_default_desc") == "\"true\"");
    CHECK(f.json(fks[1], "list_by_order_column") == "<absent>");
    CHECK(f.json("sortable_fields") == "[]");
    CHECK(f.json("order_key_columns") == R"j("{\"id\"}")j");
    CHECK(f.json("default_order_columns") == R"j("{\"id\"}")j");
    CHECK(f.json("default_order_desc") == "\"false\"");
}

/*
 * ORE Studio's account_credential; expected values are the oracle's.
 */
TEST_CASE("a stated default order names a sortable column",
    "[codegen][entity][order]") {
    codegen::tests::entity_fixture f(R"j({"entity_plural": "account_credentials",
     "default_order": "account_id",
     "primary_key": {"column": "id", "columns": [{"column": "id"}]},
     "columns": [{"name": "account_id", "sortable": true},
                 {"name": "secret"}],
     "foreign_keys": [{"column": "account_id", "list_by": true}]})j");
    derive(f);
    CHECK(f.json("sortable_fields") == R"j([
  {
    "column": "account_id",
    "name": "account_id"
  }
])j");
    CHECK(f.json("default_order_columns") == R"j("{\"account_id\"}")j");
    CHECK(f.json("default_order_desc") == "\"false\"");
    const auto fk = items(f.tables, f.entity, "foreign_keys").front();
    CHECK(f.json(fk, "list_by_order_column") == "\"id\"");
    CHECK(f.json(fk, "list_by_default_columns") ==
        R"j("{\"account_id\"}")j");
    CHECK(f.json(items(f.tables, f.entity, "extra_list_requests").front(),
        "service_method") == "\"list_account_credentials_by_account_id\"");
}

TEST_CASE("sortable_recorded_at sorts on valid_from",
    "[codegen][entity][order]") {
    codegen::tests::entity_fixture f(R"j({"sortable_recorded_at": true,
     "default_order": "recorded_at desc"})j");
    derive(f);
    CHECK(f.json("order_key_columns") == R"j("{\"id\"}")j");
    CHECK(f.json("default_order_columns") == R"j("{\"valid_from\"}")j");
    CHECK(f.json("default_order_desc") == "\"true\"");
}

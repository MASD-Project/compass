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
 * ORE Studio's notification_delivery; expected values are the oracle's.
 */
TEST_CASE("the SQL base drops _tbl and names enforced keys after it",
    "[codegen][entity][sql_names]") {
    codegen::tests::entity_fixture f(R"j({
     "product": "ores", "component": "inbox",
     "sql": {"tablename": "ores_inbox_notification_deliveries_tbl"},
     "foreign_keys": [{"column": "outcome", "enforce": true},
                      {"column": "account_id"},
                      {"column": "other", "enforce": true,
                       "constraint_name": "stated_fk"}]})j");
    codegen::ops::derive_sql_names(f.b, f.entity);
    CHECK(f.json("sql_name_base") == R"("ores_inbox_notification_deliveries")");
    CHECK(f.json("rls_table_base") == R"("notification_deliveries")");
    const auto fks = items(f.tables, f.entity, "foreign_keys");
    CHECK(f.json(fks[0], "constraint_name") ==
        R"("ores_inbox_notification_deliveries_outcome_fk")");
    CHECK(f.json(fks[1], "constraint_name") == "<absent>");
    CHECK(f.json(fks[2], "constraint_name") == R"("stated_fk")");
}

/*
 * core.py's arithmetic: a touch function's 17-character suffix leaves 46
 * characters of base, and the tenant policy's 28 leave 35 of RLS base.
 */
TEST_CASE("a long SQL base is cut so every suffix fits",
    "[codegen][entity][sql_names]") {
    codegen::tests::entity_fixture f(R"j({
     "generate_touch_function": true,
     "sql": {"tablename":
       "x_generation_config_process_parameter_value_overrides_tbl",
       "rls_tenant_isolation": true}})j");
    codegen::ops::derive_sql_names(f.b, f.entity);
    CHECK(f.json("sql_name_base") ==
        R"("x_generation_config_process_parameter_value_ov")");
    CHECK(f.json("rls_table_base") ==
        R"("x_generation_config_process_paramet")");
}

/*
 * ORE Studio's netting_set: a pin on a temporal target is checked by a
 * trigger, not a constraint.
 */
TEST_CASE("a pinned key on a temporal target is checked",
    "[codegen][entity][sql_names]") {
    codegen::tests::entity_fixture f(R"j({
     "has_tenant_id": true, "sql_name_base": "ores_refdata_netting_sets",
     "pinned_keys": [{"name": "agreement",
       "table": "ores_refdata_netting_agreements_tbl",
       "columns": ["netting_agreement_id", "counterparty_id"],
       "target_columns": ["id", "counterparty_id"]}]})j");
    codegen::types::entity_registry registry;
    registry.tables["ores_refdata_netting_agreements_tbl"] = {
        "netting_agreement", codegen::types::parent_entity{}};
    codegen::ops::resolve_pinned_keys(f.b, f.entity, registry);
    const auto pin = items(f.tables, f.entity, "pinned_keys")[0];
    CHECK(f.json(pin, "enforced") == "false");
    CHECK(f.json(pin, "matches_tenant") == "true");
    CHECK(f.json(pin, "first_column") == R"("netting_agreement_id")");
    CHECK(f.json(pin, "column_list") ==
        R"("\"tenant_id\", \"netting_agreement_id\", \"counterparty_id\"")");
    CHECK(f.json(pin, "target_column_list") ==
        R"("\"tenant_id\", \"id\", \"counterparty_id\"")");
    CHECK(f.json(pin, "constraint_name") ==
        R"("ores_refdata_netting_sets_agreement_pin")");
    CHECK(f.json(pin, "column_pairs") == R"j([
  {
    "column": "netting_agreement_id",
    "target_column": "id"
  },
  {
    "column": "counterparty_id",
    "target_column": "counterparty_id"
  }
])j");
    CHECK(items(f.tables, f.entity, "enforced_pinned_keys").empty());
    CHECK(items(f.tables, f.entity, "checked_pinned_keys").size() == 1);
}

/*
 * ORE Studio's bond_instrument, routed by the trade type catalogue; the
 * check goes before the model's own.
 */
TEST_CASE("a routed column is checked against the catalogue's codes",
    "[codegen][entity][sql_names]") {
    codegen::tests::entity_fixture f(R"j({
     "entity_singular": "bond_instrument",
     "sql": {"routed_by": "ores.trading.trade_type_catalogue",
       "routed_column": "trade_type_code",
       "extra_checks": ["\"issue_id\" is not null"]}})j");
    codegen::types::entity_registry registry;
    registry.trade_type_routes["ores.trading.trade_type_catalogue"] = {
        {"Bond", "bond_instrument"}, {"Swap", "swap_instrument"},
        {"ForwardBond", "bond_instrument"}};
    codegen::ops::resolve_routed_check(f.b, f.entity, registry);
    CHECK(f.json(member(f.tables, f.entity, "sql"), "extra_checks") == R"j([
  "\"trade_type_code\" in ('Bond', 'ForwardBond')",
  "\"issue_id\" is not null"
])j");
}

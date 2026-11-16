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
#include "compass.codegen/ops/entity_registry.hpp"
#include "entity_fixture.hpp"

namespace {

using namespace compass;
using codegen::ops::values::items;
using codegen::types::entity_registry;
using codegen::types::parent_entity;

parent_entity entity(std::string singular) {
    parent_entity p;
    p.entity_singular = std::move(singular);
    p.component = "refdata";
    return p;
}

/*
 * ORE Studio's currency pair and the tables its mandatory keys reference.
 */
entity_registry refdata_registry() {
    entity_registry r;
    auto pair = entity("currency_pair");
    pair.mandatory_fks = {
        {"base_currency", "ores_refdata_currencies_tbl", "iso_code", false},
        {"quote_currency", "ores_refdata_currencies_tbl", "iso_code", false},
        {"classification", "ores_refdata_currency_pair_classifications_tbl",
         "code", false}};
    r.tables["ores_refdata_currency_pairs_tbl"] = {"currency_pair", pair};
    r.tables["ores_refdata_currencies_tbl"] = {"currency", entity("currency")};
    r.tables["ores_refdata_currency_pair_classifications_tbl"] = {
        "currency_pair_classification",
        entity("currency_pair_classification")};
    r.tables["ores_refdata_parties_tbl"] = {"party", entity("party")};
    auto outcome = entity("outcome");
    outcome.immutable = true;
    r.tables["ores_inbox_outcomes_tbl"] = {"outcome", outcome};
    return r;
}

}

/*
 * ORE Studio's currency_pair_convention; expected values are the oracle's.
 * Both legs of the pair reference currency, so the second leg's variable
 * falls back to its column.
 */
TEST_CASE("a mandatory foreign key plans its parent's ancestors",
    "[codegen][entity][seeds]") {
    codegen::tests::entity_fixture f(R"j({
     "columns": [{"name": "pip_factor"}],
     "foreign_keys": [{"column": "pair_code", "nullable": false,
       "table": "ores_refdata_currency_pairs_tbl",
       "target_column": "pair_code"}]})j");
    codegen::ops::derive_foreign_key_seeds(f.b, f.entity, refdata_registry());
    const auto fk = items(f.tables, f.entity, "foreign_keys")[0];
    CHECK(f.json(fk, "parent_entity_singular") == R"("currency_pair")");
    CHECK(f.json(fk, "parent_component_include") == R"("refdata.api")");
    CHECK(f.json(fk, "parent_generator_facet_name") == R"("generators")");
    CHECK(f.json(fk, "target_immutable") == "false");
    CHECK(f.json(fk, "group_prefix") == R"("")");
    const auto required = items(f.tables, fk, "parent_required_fks");
    REQUIRE(required.size() == 3);
    CHECK(f.json(required[0], "var") ==
        R"("pair_code_parent_currency_parent")");
    CHECK(f.json(required[1], "var") ==
        R"("pair_code_parent_quote_currency_parent")");
    CHECK(f.json(required[2], "var") ==
        R"("pair_code_parent_currency_pair_classification_parent")");
    CHECK(f.json(required[1], "parent_var") == R"("pair_code_parent")");
    CHECK(f.json(required[1], "target_column") == R"("iso_code")");
    CHECK(f.json(required[1], "parent_component_core") == R"("refdata.core")");
    CHECK(f.json("seed_party") == "false");
    CHECK(f.json("has_system_tenant_fks") == "false");
}

TEST_CASE("an enforced key on an immutable target skips seeding",
    "[codegen][entity][seeds]") {
    codegen::tests::entity_fixture f(R"j({
     "foreign_keys": [
       {"column": "outcome", "enforce": true, "use_no_tenant": true,
        "table": "ores_inbox_outcomes_tbl"},
       {"column": "party_id", "table": "ores_refdata_parties_tbl"},
       {"column": "note_id", "nullable": true, "table": "ores_x_tbl"}]})j");
    codegen::ops::derive_foreign_key_seeds(f.b, f.entity, refdata_registry());
    const auto fks = items(f.tables, f.entity, "foreign_keys");
    CHECK(f.json(fks[0], "target_immutable") == "true");
    CHECK(f.json(fks[0], "target_no_validity") == "true");
    CHECK(f.json(fks[0], "matches_tenant") == "false");
    CHECK(f.json(fks[0], "skip_check") == "true");
    CHECK(f.json(fks[0], "parent_entity_singular") == "<absent>");
    CHECK(f.json(fks[1], "parent_is_party") == "true");
    CHECK(f.json(fks[2], "parent_entity_singular") == "<absent>");
    CHECK(f.json(fks[2], "group_prefix") == R"("")");
    CHECK(f.json("seed_party") == "true");
    codegen::ops::project_enforced_foreign_keys(f.b, f.entity);
    const auto enforced = items(f.tables, f.entity, "enforced_foreign_keys");
    REQUIRE(enforced.size() == 1);
    CHECK(f.json(enforced[0], "column") == R"("outcome")");
}

TEST_CASE("an entity without foreign keys gains no seeding flags",
    "[codegen][entity][seeds]") {
    codegen::tests::entity_fixture f(R"j({"foreign_keys": []})j");
    codegen::ops::derive_foreign_key_seeds(f.b, f.entity, refdata_registry());
    codegen::ops::project_enforced_foreign_keys(f.b, f.entity);
    CHECK(f.json("seed_party") == "<absent>");
    CHECK(f.json("enforced_foreign_keys") == "<absent>");
}

TEST_CASE("an org file claims the first table name after its singular",
    "[codegen][entity][registry]") {
    const auto claim = codegen::ops::entity_table_claim_of(
        "#+title: x\n#+entity_singular: currency\n* SQL\n"
        ":PROPERTIES:\n:tablename:     ores_refdata_currencies_tbl\n:END:\n");
    REQUIRE(claim);
    CHECK(claim->table == "ores_refdata_currencies_tbl");
    CHECK(claim->entity_singular == "currency");
    CHECK_FALSE(codegen::ops::entity_table_claim_of(
        "A doc naming =:tablename:= x_tbl\n:tablename: x_tbl\n"));
    CHECK_FALSE(codegen::ops::entity_table_claim_of(
        "#+entity_singular: doc\nthe =:tablename:= drawer\n"));
}

/*
 * A trade-style entity: grouped fields, an identity group and an audit
 * group decide the members its columns are reached through.
 */
TEST_CASE("a loaded parent maps its columns to their members",
    "[codegen][entity][registry]") {
    codegen::tests::entity_fixture f(R"j({"domain_entity": {
     "entity_singular": "trade", "component": "trading",
     "domain_identity_group": "trade_identity", "domain_audit_group": "audit",
     "domain_group_fields": [{"name": "parties.book_id"}],
     "has_tenant_id": true,
     "sql": {"immutable": true, "rls_party_isolation": true},
     "columns": [{"name": "external_id", "group": "identity"},
                 {"name": "book_id", "group": "identity"}],
     "primary_key": {"columns": [{"column": "id", "group": "identity"}]},
     "indexes": [{"columns": "tenant_id, external_id", "unique": true},
                 {"columns": "lower(name)", "unique": true}],
     "foreign_keys": [{"column": "book_id", "table": "b_tbl"},
                      {"column": "x", "nullable": true}]}})j");
    const auto p = codegen::ops::parent_entity_of(f.tables, f.loaded);
    CHECK(p.column_prefixes.at("book_id") == "parties.");
    CHECK(p.column_prefixes.at("external_id") == "identity.");
    CHECK(p.column_prefixes.at("id") == "identity.");
    CHECK(p.column_prefixes.at("modified_by") == "audit.");
    CHECK(p.has_audit_group);
    CHECK(p.has_identity_group);
    CHECK(p.immutable_tenant_key);
    CHECK(p.party_isolated);
    REQUIRE(p.mandatory_fks.size() == 1);
    CHECK(p.mandatory_fks[0].table == "b_tbl");
    REQUIRE(p.unique_keys.size() == 2);
    CHECK(p.unique_keys[0] == std::set<std::string>{"id", "tenant_id"});
    CHECK(p.unique_keys[1] ==
        std::set<std::string>{"external_id", "tenant_id"});
    CHECK(p.column_names.contains("id"));
}

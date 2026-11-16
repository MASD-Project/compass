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

#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "entity_fixture.hpp"

namespace {

using namespace compass;
using strings = std::vector<std::string>;

strings names(codegen::tests::entity_fixture& f, std::string_view key) {
    strings out;
    for (const auto v : codegen::ops::values::items(f.tables, f.entity, key))
        out.emplace_back(codegen::ops::values::py_str(f.tables, v));
    return out;
}

/*
 * ores.trading.bond_instrument as the oracle shows it: the identity group
 * carries three columns, which reach no flat member and declare nothing.
 */
constexpr auto bond = R"j({
    "has_identity_group": true,
    "domain_identity_group": "ores.trading.instrument_identity",
    "domain_audit_group": "ores.dq.audit_record",
    "columns": [
        {"name": "trade_type_code", "is_identity_group_column": true},
        {"name": "party_id", "is_identity_group_column": true},
        {"name": "issue_id", "economic": false},
        {"name": "position_identifier", "economic": false},
        {"name": "notional", "economic": true}]})j";

}

TEST_CASE("a struct declaring its members lists the economic ones",
    "[codegen][entity][economics]") {
    codegen::tests::entity_fixture f(bond);
    codegen::ops::derive_economic_declaration(f.b, f.entity, {});
    CHECK(f.json("has_economic_declaration") == "true");
    CHECK(names(f, "economic_fields") == strings{"notional"});
    CHECK(f.json("has_economic_fields") == "true");
    CHECK(names(f, "canonical_members") == strings{"notional"});
    CHECK(f.json("has_canonical_members") == "true");
}

TEST_CASE("groups declaring economic fields join the canonical form",
    "[codegen][entity][economics]") {
    codegen::tests::entity_fixture f(bond);
    codegen::types::entity_registry registry;
    registry.economic_field_groups = {"ores.trading.instrument_identity",
        "ores.dq.audit_record"};
    codegen::ops::derive_economic_declaration(f.b, f.entity, registry);
    CHECK(names(f, "economic_fields") == strings{"notional"});
    CHECK(names(f, "canonical_members") ==
        strings{"identity", "notional", "audit"});
}

TEST_CASE("a struct declaring nothing has no canonical form",
    "[codegen][entity][economics]") {
    codegen::tests::entity_fixture f(R"j({
        "primary_key": {"columns": [{"column": "id"}]},
        "columns": [{"name": "code"}]})j");
    codegen::ops::derive_economic_declaration(f.b, f.entity, {});
    CHECK(f.json("has_economic_declaration") == "false");
    CHECK(f.json("economic_fields") == "[]");
    CHECK(f.json("has_economic_fields") == "false");
    CHECK(f.json("canonical_members") == "<absent>");
}

TEST_CASE("a half-declared struct is refused",
    "[codegen][entity][economics]") {
    codegen::tests::entity_fixture f(R"j({
        "primary_key": {"columns": [{"column": "id"}]},
        "columns": [{"name": "notional", "economic": true}]})j");
    CHECK_THROWS(codegen::ops::derive_economic_declaration(f.b, f.entity, {}));
}

TEST_CASE("a grouped entity declares through its groups",
    "[codegen][entity][economics]") {
    codegen::tests::entity_fixture f(R"j({"has_domain_groups": true,
        "domain_groups": [
            {"member": "identity", "field_group": "ores.a.identity"},
            {"member": "leg", "field_group": "ores.a.leg"}]})j");
    codegen::types::entity_registry registry;
    registry.economic_field_groups = {"ores.a.leg"};
    codegen::ops::derive_economic_declaration(f.b, f.entity, registry);
    CHECK(f.json("has_economic_declaration") == "true");
    CHECK(f.json("economic_fields") == "[]");
    CHECK(names(f, "canonical_members") == strings{"leg"});
}

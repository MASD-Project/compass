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

void derive_groups(codegen::tests::entity_fixture& f) {
    codegen::ops::derive_field_groups(f.b, f.entity);
    codegen::ops::derive_group_prefixes(f.b, f.entity);
    codegen::ops::derive_stamp_members(f.b, f.entity);
    codegen::ops::derive_key_group_prefixes(f.b, f.entity);
    codegen::ops::inject_group_includes(f.b, f.entity);
}

}

/*
 * ORE Studio's bond_instrument, trimmed; expected values are the oracle's.
 */
TEST_CASE("an identity and audit grouped entity reaches its groups' members",
    "[codegen][entity][groups]") {
    codegen::tests::entity_fixture f(R"j({"component": "trading",
     "domain_identity_group": "ores.trading.instrument_identity",
     "domain_audit_group": "ores.dq.audit_record",
     "columns": [{"name": "party_id", "group": "identity"},
                 {"name": "issue_id"}],
     "primary_key": {"column": "id", "group": "identity",
       "columns": [{"column": "id", "group": "identity"}]},
     "cpp": {"includes": {"domain": ["<string>",
       "\"ores.utility/decimal/decimal.hpp\""]}}})j");
    derive_groups(f);

    CHECK(f.json("has_identity_group") == "true");
    CHECK(f.json("has_audit_group") == "true");
    CHECK(f.json("identity_group_type") == "\"instrument_identity\"");
    CHECK(f.json("audit_group_qualified") ==
        "\"ores::dq::domain::audit_record\"");
    CHECK(f.json("has_domain_groups") == "false");
    CHECK(f.json("scaffold_prefix") == "\"identity.\"");
    CHECK(f.json("audit_prefix") == "\"audit.\"");
    CHECK(f.json("stamp_members") == R"j([
  {
    "member": "identity"
  },
  {
    "member": "audit"
  }
])j");
    CHECK(f.json("has_stamp_members") == "true");

    const auto columns = items(f.tables, f.entity, "columns");
    CHECK(f.json(columns[0], "group_prefix") == "\"identity.\"");
    CHECK(f.json(columns[0], "is_identity_group_column") == "true");
    CHECK(f.json(columns[1], "group_prefix") == "\"\"");
    CHECK(f.json(columns[1], "is_identity_group_column") == "false");
    const auto pk = member(f.tables, f.entity, "primary_key");
    CHECK(f.json(pk, "group_prefix") == "\"identity.\"");
    CHECK(f.json(items(f.tables, pk, "columns").front(),
        "is_identity_group_column") == "true");

    CHECK(f.json(member(f.tables, f.entity, "cpp"), "includes") == R"j({
  "domain": [
    "\"ores.dq.api/domain/audit_record.hpp\"",
    "\"ores.trading.api/domain/instrument_identity.hpp\"",
    "<string>",
    "\"ores.utility/decimal/decimal.hpp\""
  ]
})j");
}

/*
 * ORE Studio's convention, trimmed; expected values are the oracle's.
 */
TEST_CASE("a domain-grouped entity reaches every column through a member",
    "[codegen][entity][groups]") {
    codegen::tests::entity_fixture f(R"j({"component": "refdata",
     "domain_groups": [
      {"field_group": "ores.refdata.convention_identity", "member": "identity"},
      {"field_group": "ores.refdata.convention_payload", "member": "payload"},
      {"field_group": "ores.dq.audit_record", "member": "audit"}],
     "domain_group_fields": [{"name": "identity.version"},
      {"name": "identity.party_id"}, {"name": "payload.oresmd_uri"},
      {"name": "audit.modified_by"}, {"name": "audit.recorded_at"}],
     "columns": [{"name": "oresmd_uri"}]})j");
    derive_groups(f);

    const auto groups = items(f.tables, f.entity, "domain_groups");
    CHECK(f.json(groups[0], "type_qualified") == "\"convention_identity\"");
    CHECK(f.json(groups[0], "header") ==
        R"j("\"ores.refdata.api/domain/convention_identity.hpp\"")j");
    CHECK(f.json(groups[2], "type_qualified") ==
        "\"ores::dq::domain::audit_record\"");
    CHECK(f.json("has_domain_groups") == "true");
    CHECK(f.json("has_identity_group") == "false");
    CHECK(f.json("scaffold_prefix") == "\"identity.\"");
    CHECK(f.json("audit_prefix") == "\"audit.\"");
    CHECK(f.json(items(f.tables, f.entity, "columns").front(),
        "group_prefix") == "\"payload.\"");
    CHECK(f.json("stamp_members") == R"j([
  {
    "member": "identity"
  },
  {
    "member": "audit"
  }
])j");
    CHECK(f.json(member(f.tables, f.entity, "cpp"), "includes") == R"j({
  "domain": [
    "\"ores.dq.api/domain/audit_record.hpp\"",
    "\"ores.refdata.api/domain/convention_identity.hpp\"",
    "\"ores.refdata.api/domain/convention_payload.hpp\""
  ]
})j");
}

TEST_CASE("a flat entity has no group prefixes, stamps or injected includes",
    "[codegen][entity][groups]") {
    codegen::tests::entity_fixture f(R"j({"columns": [{"name": "code"}],
     "cpp": {"includes": {"domain": ["<string>"]}}})j");
    derive_groups(f);
    CHECK(f.json("scaffold_prefix") == "\"\"");
    CHECK(f.json("stamp_members") == "[]");
    CHECK(f.json("has_stamp_members") == "false");
    CHECK(f.json(member(f.tables, f.entity, "cpp"), "includes") == R"j({
  "domain": [
    "<string>"
  ]
})j");
}

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

codegen::types::entity_registry registry_with_custom_type() {
    codegen::types::entity_registry r;
    r.custom_types = {{"domain::cron_expression", "ores.utility/cron.hpp"},
        {"ores::trading::domain::origin", "ores.trading.api/domain/origin.hpp"}};
    return r;
}

}

TEST_CASE("members take the TypeScript type of their domain type",
    "[codegen][entity][typescript]") {
    codegen::tests::entity_fixture f(R"j({
        "columns": [
            {"name": "id", "cpp_type": "std::optional<boost::uuids::uuid>"},
            {"name": "spans",
             "cpp_type": "std::vector<ores::diff::domain::diff_span>"},
            {"name": "origin", "cpp_type": "ores::trading::domain::origin"},
            {"name": "socket", "cpp_type": "boost::asio::ip::tcp"}],
        "natural_keys": [{"column": "day",
            "cpp_type": "std::chrono::year_month_day"}],
        "primary_key": {"cpp_type": "int", "columns": [{"cpp_type": "int"}]}})j");
    const auto registry = registry_with_custom_type();
    codegen::ops::derive_ts_types(f.b, f.entity, registry);
    const auto columns = items(f.tables, member(f.tables, f.entity, "columns"));
    CHECK(f.json(columns[0], "ts_type") == R"("string | null")");
    CHECK(f.json(columns[1], "ts_type") == R"("DiffSpan[]")");
    CHECK(f.json(columns[2], "ts_type") == R"("string")");
    CHECK(f.json(columns[3], "ts_type") == "<absent>");
    const auto keys = items(f.tables, member(f.tables, f.entity, "natural_keys"));
    CHECK(f.json(keys[0], "ts_type") == R"("string")");
    const auto pk = member(f.tables, f.entity, "primary_key");
    CHECK(f.json(pk, "ts_type") == R"("number")");
}

/*
 * ORE Studio's convention: its own groups sit beside it, dq's audit record
 * under dq.
 */
TEST_CASE("groups import from beside the entity or their component",
    "[codegen][entity][typescript]") {
    codegen::tests::entity_fixture f(R"j({"component": "refdata",
        "domain_groups": [
            {"type_qualified": "convention_identity",
             "field_group": "ores.refdata.convention_identity"},
            {"type_qualified": "ores::dq::domain::audit_record",
             "field_group": "ores.dq.audit_record"}],
        "has_audit_group": true,
        "audit_group_qualified": "ores::dq::domain::audit_record"})j");
    codegen::ops::derive_ts_group_imports(f.b, f.entity);
    const auto groups = items(f.tables,
        member(f.tables, f.entity, "domain_groups"));
    CHECK(f.json(groups[0], "ts_type") == R"("ConventionIdentity")");
    CHECK(f.json(groups[0], "ts_import") == R"("./convention_identity.js")");
    CHECK(f.json(groups[1], "ts_type") == R"("AuditRecord")");
    CHECK(f.json(groups[1], "ts_import") ==
        R"("../../dq/domain/audit_record.js")");
    CHECK(f.json("audit_group_pascal") == R"("AuditRecord")");
    CHECK(f.json("audit_group_import") ==
        R"("../../dq/domain/audit_record.js")");
    CHECK(f.json("identity_group_pascal") == "<absent>");
}

/*
 * ORE Studio's diff_entry imports diff_span from beside it; a group's
 * interface and the entity's own type are not imported again.
 */
TEST_CASE("column imports name each member entity's module once",
    "[codegen][entity][typescript]") {
    codegen::tests::entity_fixture f(R"j({"component": "diff",
        "entity_singular": "diff_entry",
        "domain_groups": [{"ts_type": "AuditRecord"}],
        "columns": [
            {"cpp_type": "std::vector<ores::diff::domain::diff_span>"},
            {"cpp_type": "std::optional<ores::diff::domain::diff_span>"},
            {"cpp_type": "ores::diff::domain::diff_entry"},
            {"cpp_type": "ores::dq::domain::audit_record"},
            {"cpp_type": "ores::iam::domain::account"},
            {"cpp_type": "domain::cron_expression"}]})j");
    const auto registry = registry_with_custom_type();
    codegen::ops::derive_ts_column_imports(f.b, f.entity, registry);
    CHECK(f.json("ts_column_imports") == R"j([
  {
    "module": "../../iam/domain/account.js",
    "pascal": "Account"
  },
  {
    "module": "./diff_span.js",
    "pascal": "DiffSpan"
  }
])j");
}

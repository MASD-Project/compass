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

}

/*
 * ORE Studio's country declares coding_scheme_code by hand under a nullable
 * coding scheme; the oracle keeps only has_any_coding_scheme.
 */
TEST_CASE("a hand-declared coding scheme column clears the column flags",
    "[codegen][entity][markers]") {
    codegen::tests::entity_fixture f(R"j({"coding_scheme": "nullable",
        "columns": [{"name": "alpha3_code"}, {"name": "coding_scheme_code"}]})j");
    codegen::ops::derive_sql_table_flags(f.b, f.entity);
    codegen::ops::suppress_manual_coding_scheme(f.b, f.entity);
    CHECK(f.json("has_coding_scheme") == "false");
    CHECK(f.json("has_nullable_coding_scheme") == "false");
    CHECK(f.json("has_any_coding_scheme") == "true");
}

TEST_CASE("without a hand-declared column the coding scheme flags stay",
    "[codegen][entity][markers]") {
    codegen::tests::entity_fixture f(R"j({"coding_scheme": "nullable",
        "columns": [{"name": "alpha3_code"}]})j");
    codegen::ops::derive_sql_table_flags(f.b, f.entity);
    codegen::ops::suppress_manual_coding_scheme(f.b, f.entity);
    CHECK(f.json("has_nullable_coding_scheme") == "true");
}

TEST_CASE("the artefact lists mark their last item",
    "[codegen][entity][markers]") {
    codegen::tests::entity_fixture f(R"j({
        "artefact_indexes": [{"name": "a"}, {"name": "b"}],
        "artefact_columns": [{"name": "c"}]})j");
    codegen::ops::mark_artefact_last_items(f.b, f.entity);
    CHECK(f.json("artefact_indexes") == R"j([
  {
    "name": "a"
  },
  {
    "last": true,
    "name": "b"
  }
])j");
    CHECK(f.json("artefact_columns") == R"j([
  {
    "last": true,
    "name": "c"
  }
])j");
}

/*
 * ORE Studio's party_identifier limits identifiers per scheme.
 */
TEST_CASE("a cardinality-limit validation flags the entity's sql",
    "[codegen][entity][markers]") {
    codegen::tests::entity_fixture f(R"j({"insert_trigger": {"validations": [
        {"column": "change_reason_code"},
        {"column": "id_scheme",
         "cardinality_limit_table": "ores_refdata_party_id_schemes_tbl"}]}})j");
    codegen::ops::derive_cardinality_limit_flag(f.b, f.entity);
    CHECK(f.json("sql") == R"j({
  "has_cardinality_limit_validations": true
})j");

    codegen::tests::entity_fixture plain(R"j({"insert_trigger":
        {"validations": [{"column": "change_reason_code"}]}})j");
    codegen::ops::derive_cardinality_limit_flag(plain.b, plain.entity);
    CHECK(plain.json("sql") == "<absent>");
}

/*
 * ORE Studio's role: the oracle's is_requestable detail.
 */
TEST_CASE("a multi-line column detail takes the doxygen prefix",
    "[codegen][entity][markers]") {
    codegen::tests::entity_fixture f(R"j({"columns": [
        {"name": "is_requestable", "detail": "A service's role.\n\nThe flag is read with the role."},
        {"name": "code", "detail": "One line."}]})j");
    codegen::ops::format_columns(f.b, f.entity);
    const auto columns = items(f.tables, f.entity, "columns");
    CHECK(f.json(columns.front(), "detail") ==
        R"j("A service's role.\n     *\n     * The flag is read with the role.")j");
    CHECK(f.json(columns.front(), "last") == "<absent>");
    CHECK(f.json(columns.back(), "detail") == R"j("One line.")j");
    CHECK(f.json(columns.back(), "last") == "true");
}

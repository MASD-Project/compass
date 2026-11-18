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
#include <catch2/catch_test_macros.hpp>
#include "compass.crosscheck/codecs/generated_column_refs.hpp"
#include "entity_model.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;
using tests::model_column;

const std::string path = "projects/ores.refdata/modeling/ores.refdata.currency.org";

const std::string columns =
    model_column("code", ":type: text\n:cpp_type: std::string\n"
        ":primary_key: true\n") +
    model_column("name", ":type: text\n:cpp_type: std::string\n") +
    model_column("digits", ":type: integer\n:cpp_type: int\n");

std::string currency(std::string_view display, std::string_view indexes,
    std::string_view drawer = {}) {
    return tests::entity_model("refdata", "currency",
        "ores_refdata_currencies_tbl", columns, "** Indexes\n\n"
        "| name | columns |\n|------+---------|\n" + std::string(indexes) +
        "\n* C++\n\n** Table display\n\n| column | header |\n"
        "|--------+--------|\n" + std::string(display), ":has_tenant_id: "
        "true\n", drawer);
}

std::vector<std::pair<std::string, std::uint32_t>> places(
    const tests::outcome& r) {
    return tests::places(r.findings);
}

}

TEST_CASE("a table display or index naming a column the model lacks is "
    "reported on the model", "[codecs][generated_column_refs]") {
    const files t{{path, currency(
        "| code | Code |\n| name | Name |\n| version | Version |\n"
        "| modified_by | By |\n| symbol | Symbol |\n",
        "| by_name | name, lower(name) |\n| by_tenant | tenant_id, "
        "\"digits\" desc |\n| by_symbol | symbol, valid_to |\n")}};
    const auto r = tests::run(codecs::generated_column_refs, t);
    CHECK(tests::messages_of(r) == std::vector<std::string>{
        "`e.symbol` is not a member of currency",
        "index by_symbol names symbol, which is not a column of "
        "ores_refdata_currencies_tbl"});
    CHECK(places(r) == std::vector<std::pair<std::string, std::uint32_t>>{
        {path, 55}, {path, 45}});
}

TEST_CASE("a current-state table has no audit or validity columns to name",
    "[codecs][generated_column_refs]") {
    const files t{{path, tests::entity_model("refdata", "currency",
        "ores_refdata_currencies_tbl", columns, "** Indexes\n\n"
        "| name | columns |\n|------+---------|\n| by_time | valid_to |\n"
        "\n* C++\n\n** Table display\n\n| column | header |\n"
        "|--------+--------|\n| version | Version |\n", {}, {},
        ":current_state: true\n")}};
    CHECK(tests::messages_of(tests::run(codecs::generated_column_refs, t)) ==
        std::vector<std::string>{
            "`e.version` is not a member of currency",
            "index by_time names valid_to, which is not a column of "
            "ores_refdata_currencies_tbl"});
}

TEST_CASE("a field of an identity group is a member through the group",
    "[codecs][generated_column_refs]") {
    const files t{
        {path, tests::entity_model("refdata", "currency",
            "ores_refdata_currencies_tbl", columns, "* C++\n\n"
            "** Table display\n\n| column | header |\n|--------+--------|\n"
            "| identity.code | Code |\n| identity.gone | Gone |\n"
            "| name | Name |\n",
            ":has_tenant_id: true\n:domain_identity_group: "
            "ores.refdata.currency_identity\n")},
        {"projects/ores.refdata/modeling/"
         "ores.refdata.currency_identity_field_group.org",
         "#+title: ores.refdata.currency_identity\n"
         "#+type: ores.codegen.field_group\n#+component: refdata\n"
         "#+name_singular: currency_identity\n\n* Fields\n\n" +
         model_column("code", ":cpp_type: std::string\n")}};
    CHECK(tests::messages_of(tests::run(codecs::generated_column_refs, t)) ==
        std::vector<std::string>{
            "`e.identity.gone` is not a member of currency"});
}

TEST_CASE("what code generation does not render is not checked",
    "[codecs][generated_column_refs]") {
    const auto broken = currency("| symbol | Symbol |\n",
        "| by_symbol | symbol |\n",
        ":ores.cpp.domain.table_impl.enabled: false\n"
        ":ores.sql.schema.enabled: false\n");
    const files t{{path, broken},
        {"doc/agile/notes/ores.refdata.currency.org", currency(
            "| symbol | Symbol |\n", "| by_symbol | symbol |\n")}};
    CHECK(tests::run(codecs::generated_column_refs, t).findings.empty());
}

TEST_CASE("an edit to a model checks that model again",
    "[codecs][generated_column_refs]") {
    const files before{{path, currency("| name | Name |\n",
        "| by_name | name |\n")}};
    auto after = before;
    after[path] = tests::entity_model("refdata", "currency",
        "ores_refdata_currencies_tbl", model_column("code", ":type: text\n"
            ":cpp_type: std::string\n:primary_key: true\n") +
        model_column("title", ":type: text\n:cpp_type: std::string\n"),
        "** Indexes\n\n| name | columns |\n|------+---------|\n"
        "| by_name | name |\n\n* C++\n\n** Table display\n\n"
        "| column | header |\n|--------+--------|\n| name | Name |\n");
    CHECK(tests::run(codecs::generated_column_refs, before).findings.empty());
    CHECK(tests::messages_of(tests::run_after_edit(
        codecs::generated_column_refs, before, after)) ==
        std::vector<std::string>{"`e.name` is not a member of currency",
            "index by_name names name, which is not a column of "
            "ores_refdata_currencies_tbl"});
}

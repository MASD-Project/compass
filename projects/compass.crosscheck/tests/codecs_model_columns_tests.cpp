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
#include "compass.crosscheck/codecs/entity_binary_types.hpp"
#include "compass.crosscheck/codecs/model_sql_types.hpp"
#include "compass.crosscheck/codecs/money_decimal_columns.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

std::vector<std::string> messages(const tests::outcome& r) {
    std::vector<std::string> out;
    for (const auto& [path, line, message] : r.findings)
        out.push_back(path + ":" + std::to_string(line) + ": " + message);
    return out;
}

}

TEST_CASE("a money column typed inexactly and a continuous one typed exact "
    "are reported", "[codecs][money_decimal_columns]") {
    const files t{
        {"projects/ores.trading/modeling/ores.trading.swap.org",
         "* Columns\n"
         "** notional_amount\n:type: double precision\n:cpp_type: double\n"
         "** strike\n:type: numeric(38,12)\n:cpp_type: decimal\n"
         "** face_value\n:type: text\n:cpp_type: std::string\n"
         "** recovery_rate\n:type: numeric\n"
         "** premium_date\n:type: date\n"
         "** price\n:type: NUMERIC(38, 12)\n:cpp_type: std::optional<double>\n"
         "* Left\n:column: margin\n:type: real\n"},
        {"projects/ores.trading/core/modeling/ores.trading.fee_field_group.org",
         "** fee\n:cpp_type: std::string\n** weight\n:cpp_type: decimal\n"},
        {"projects/ores.trading/modeling/ores.trading.fx_variance_swap_instrument.org",
         "** strike\n:type: numeric\n"},
        {"projects/ores.synthetic/modeling/ores.synthetic.ir_curve_generation_config.org",
         "** initial_rate\n:type: double precision\n"}};
    CHECK(messages(tests::run(codecs::money_decimal_columns, t,
            {{"min_columns", "1"}})) == std::vector<std::string>{
        "projects/ores.trading/core/modeling/ores.trading.fee_field_group.org:1: "
        "fee is money, typed sql=(embedded) cpp=std::string",
        "projects/ores.trading/core/modeling/ores.trading.fee_field_group.org:3: "
        "weight is a continuous quantity, typed sql=(embedded) cpp=decimal",
        "projects/ores.trading/modeling/ores.trading.fx_variance_swap_instrument.org:1: "
        "strike is a continuous quantity, typed sql=numeric cpp=-",
        "projects/ores.trading/modeling/ores.trading.swap.org:2: "
        "notional_amount is money, typed sql=double precision cpp=double",
        "projects/ores.trading/modeling/ores.trading.swap.org:8: "
        "face_value is money, typed sql=text cpp=std::string",
        "projects/ores.trading/modeling/ores.trading.swap.org:11: "
        "recovery_rate is a continuous quantity, typed sql=numeric cpp=-",
        "projects/ores.trading/modeling/ores.trading.swap.org:15: "
        "price is money, typed sql=NUMERIC(38, 12) "
        "cpp=std::optional<double>",
        "projects/ores.trading/modeling/ores.trading.swap.org:19: "
        "margin is money, typed sql=real cpp=-"});
}

TEST_CASE("a tree with too few columns is a finding, not a pass",
    "[codecs][money_decimal_columns]") {
    const files t{{"projects/ores.a/modeling/a.org", "** amount\n"}};
    const auto r = tests::run(codecs::money_decimal_columns, t);
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<0>(r.findings[0]) == "projects");
}

TEST_CASE("a model type that differs from its created type is reported",
    "[codecs][model_sql_types]") {
    const files t{
        {"projects/ores.iam/modeling/ores.iam.role.org",
         ":tablename: ores_iam_roles_tbl\n* Columns\n"
         "** id\n:type: uuid\n** name\n:type: text\n"
         "** amount\n:type: numeric(38, 12)\n** gone\n:type: text\n"
         "** level\n:type: Integer\n* Other\n** stray\n:type: text\n"},
        {"projects/ores.sql/create/iam/iam_roles_create.sql",
         "-- header\ncreate table if not exists \"ores_iam_roles_tbl\" (\n"
         "    \"id\" uuid not null,\n    \"name\" varchar(10) default 'x',\n"
         "    \"amount\" numeric(38,12) not null,\n"
         "    -- comment\n    \"level\" integer,\n"
         "    \"stray\" bigint\n);\n"}};
    CHECK(tests::messages_of(tests::run(codecs::model_sql_types, t,
            {{"min_compared", "1"}})) == std::vector<std::string>{
        "name: the model types it 'text' but "
        "projects/ores.sql/create/iam/iam_roles_create.sql creates it "
        "'varchar(10)'",
        "amount: the model types it 'numeric(38, 12)' but "
        "projects/ores.sql/create/iam/iam_roles_create.sql creates it "
        "'numeric(38,12)'"});
}

TEST_CASE("a row binds each column in its table's binary format",
    "[codecs][entity_binary_types]") {
    const files t{
        {"projects/ores.sql/create/iam/iam_roles_create.sql",
         "create table \"ores_iam_roles_tbl\" (\n"
         "  \"id\" uuid not null,\n  \"version\" integer not null,\n"
         "  \"amount\" numeric(38,12),\n  \"kind\" ores_kind_t,\n"
         "  \"size\" bigint\n);\n"
         "create or replace view ores_iam_roles_vw as select 1;\n"},
        {"projects/ores.iam/core/include/ores.iam.core/repository/role_entity.hpp",
         "// AUTO-GENERATED FILE\nstruct role_entity {\n"
         "    constexpr static const char* tablename = \"ores_iam_roles_tbl\";\n"
         "    static constexpr auto columns() {\n"
         "        query_builder::key(\"id\", &e::id),\n"
         "        query_builder::bind(\"version\", &e::version),\n"
         "        query_builder::bind(\"amount\",\n            &e::amount),\n"
         "        query_builder::bind_text(\"kind\", &e::kind),\n"
         "        query_builder::bind(\"size\", &e::size),\n"
         "        query_builder::bind(\"missing\", &e::id),\n"
         "    }\n"
         "    boost::uuids::uuid id;\n    std::int64_t version = 0;\n"
         "    std::optional<double> amount;\n    std::string kind;\n"
         "    int size{};\n};\n"},
        {"projects/ores.iam/core/include/ores.iam.core/repository/view_entity.hpp",
         "// AUTO-GENERATED FILE\n"
         "constexpr static const char* tablename = \"ores_iam_roles_vw\";\n"
         "static constexpr auto columns()\n"},
        {"projects/ores.iam/core/include/ores.iam.core/repository/lost_entity.hpp",
         "// AUTO-GENERATED FILE\n"
         "constexpr static const char* tablename = \"ores_iam_lost_tbl\";\n"
         "static constexpr auto columns()\n"}};
    CHECK(tests::messages_of(tests::run(codecs::entity_binary_types, t,
            {{"min_compared", "1"}})) == std::vector<std::string>{
        "no create script creates ores_iam_lost_tbl",
        "'version' holds std::int64_t (integer 64) but ores_iam_roles_tbl "
        "creates it integer (integer 32)",
        "'amount' holds std::optional<double> (double precision) but "
        "ores_iam_roles_tbl creates it numeric(38,12) (numeric)",
        "'size' holds int (integer 32) but ores_iam_roles_tbl creates it "
        "bigint (integer 64)",
        "the row binds 'missing', which ores_iam_roles_tbl does not create"});
}

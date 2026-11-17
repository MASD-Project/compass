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
#include "compass.crosscheck/codecs/conventions_dataset.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.crosscheck/ops/python_numbers.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

const std::string here = "projects/ores.seeder/datasets/conventions/";
const std::string output = "projects/ores.sql/populate/refdata/"
    "refdata_conventions_artefact_populate.sql";

const std::string script =
    "DAY_COUNTER = {\"A365\": \"ACT/365.FIXED\"}\n"
    "BDC = {\"MF\": \"ModifiedFollowing\"}\n"
    "FREQUENCY = {\"Annual\": \"Annual\"}\n"
    "COMPOUNDING = {}\n"
    "DATE_RULE = {}\n"
    "KINDS = {\n"
    "    \"Deposit\": (\"deposit\", [\n"
    "        (\"Index\", \"index\", \"text\"),\n"
    "        (\"SpotDays\", \"spot_days\", \"int\"),\n"
    "        (\"Rate\", \"rate\", \"float\"),\n"
    "        (\"DayCounter\", \"day_count_fraction\", \"daycounter\"),\n"
    "    ]),\n"
    "    \"IborIndex\": (\"ibor_index\", [\n"
    "        (\"EOM\", \"eom\", \"bool\"),\n"
    "    ]),\n"
    "}\n"
    "CUT_KINDS = {\n"
    "    \"FX\": \"world data \"\n"
    "          \"elsewhere.\",\n"
    "}\n"
    "COMPOSITE_KINDS = {\n"
    "    \"TenorBasisSwap\": {\n"
    "        \"kind_code\": \"TENOR_BASIS_SWAP\",\n"
    "        \"central_table\": \"ores_dq_conventions_artefact_tbl\",\n"
    "        \"parts_of\": tenor_basis_swap_parts,\n"
    "        \"parts\": {\n"
    "            \"pay_receive\": {\"table\": \"pr_tbl\", \"mapping\": [\n"
    "                (\"PayIndex\", \"pay_index\", \"text\")]},\n"
    "            \"long_short\": {\"table\": \"ls_tbl\", \"mapping\": [\n"
    "                (\"LongIndex\", \"long_index\", \"text\"),\n"
    "                (\"SpreadOnShort\", \"spread_on_short\", \"bool\")]},\n"
    "        },\n"
    "    },\n"
    "    \"CrossCurrencyBasis\": {\n"
    "        \"kind_code\": \"CROSS_CURRENCY_BASIS\",\n"
    "        \"central_table\": \"ores_dq_conventions_artefact_tbl\",\n"
    "        \"parts_of\": lambda fields: [\"leg\"],\n"
    "        \"parts\": {\n"
    "            \"leg\": {\"table\": \"leg_tbl\", \"mapping\": [\n"
    "                (\"FlatIndex\", \"flat_index\", \"text\"),\n"
    "                (None, \"decoded_column\", \"decoded\")]},\n"
    "            \"unused\": {\"table\": \"unused_tbl\", \"mapping\": []},\n"
    "        },\n"
    "    },\n"
    "}\n"
    "WORLD_KINDS = {\"ibor_index\"}\n"
    "INDEX_FIELDS = (\"Index\", \"FlatIndex\", \"PayIndex\", \"LongIndex\")\n"
    "INDEX_KINDS = (\"IborIndex\",)\n"
    "PRIMARY_INDEX = {\n    \"Deposit\": (\"Index\",),\n}\n"
    "NESTED_COLUMNS = {}\n"
    "HEADER = \"\"\"-- header\n\"\"\"\n"
    "DATASET_CODE = \"refdata.conventions\"\n"
    "METHODOLOGY = \"Method\"\n"
    "METHODOLOGY_DESCRIPTION = (\"How \"\n    \"it is done\")\n"
    "METHODOLOGY_URL = \"https://example.org\"\n";

const std::string tsv =
    "kind\tid\tsignature\tsource\toresmd_uri\taliases\n"
    "Deposit\tEUR-DEP\tIndex=EUR-EURIBOR-3M ; SpotDays= 0_2 ; Rate=1e-05 ; "
    "DayCounter=A365\tx\t\t\n"
    "\n"
    "IborIndex\tEUR-EURIBOR-3M\tEOM= Yes\tx\t\t\n"
    "FX\tEUR-USD\tSpotDays=2\tx\t\t\n"
    "TenorBasisSwap\tTB-1\tLongIndex=EUR-EURIBOR-3M ; SpreadOnShort=no\tx\t\t\n"
    "TenorBasisSwap\tTB-2\tPayIndex=EUR-EURIBOR-3M\tx\t\t\n"
    "CrossCurrencyBasis\tXCCY\tFlatIndex=O'NIA\tx\t\t\n"
    "IborIndex\tO'NIA\t\tx\toresmd://onia\t\n";

files tree() {
    return {
        {here + "generate_sql.py", script},
        {here + "conventions.tsv", tsv},
        {here + "oresmd_index_map.tsv",
         "EUR-EURIBOR-3M\toresmd://euribor/3m\n\n"},
        {here + "methodology.txt", "\n  Curated by hand.\n\n"}};
}

}

TEST_CASE("Python's int() and float() read a number as the script does",
    "[codecs][conventions_dataset]") {
    CHECK(ops::python_int(" +0_42 ") == "42");
    CHECK(ops::python_int("-0") == "0");
    CHECK(ops::python_int("-07") == "-7");
    CHECK_FALSE(ops::python_int("1__0"));
    CHECK_FALSE(ops::python_int("_1"));
    CHECK_FALSE(ops::python_int("1.0"));
    CHECK(ops::python_float("1e-05") == "1e-05");
    CHECK(ops::python_float("0.0001") == "0.0001");
    CHECK(ops::python_float("1e16") == "1e+16");
    CHECK(ops::python_float("9999999999999998") == "9999999999999998.0");
    CHECK(ops::python_float(" 100 ") == "100.0");
    CHECK(ops::python_float("-0.0") == "-0.0");
    CHECK(ops::python_float("1_000.5e-3") == "1.0005");
    CHECK(ops::python_float("0.1") == "0.1");
    CHECK(ops::python_float("1.5e300") == "1.5e+300");
    CHECK(ops::python_float("Infinity") == "inf");
    CHECK_FALSE(ops::python_float("1e"));
    CHECK_FALSE(ops::python_float("abc"));
}

TEST_CASE("each kind is one block, in the order of the kinds, with the "
    "values parsed and the index's URI", "[codecs][conventions_dataset]") {
    const auto out = tests::generate(codecs::conventions_dataset, tree());
    const auto& sql = out.at(output);
    CHECK(sql.starts_with("-- header\n\n-- ====="));
    CHECK(sql.contains("        'Method',\n        'How it is done',\n"
        "        'https://example.org',\n        'Curated by hand.'\n"));
    CHECK(sql.contains("raise exception 'Dataset not found: "
        "refdata.conventions';"));
    CHECK(sql.contains(
        "\n    -- CrossCurrencyBasis central: 1 rows\n"
        "    if not exists (\n"
        "        select 1 from ores_dq_conventions_artefact_tbl\n"
        "        where dataset_id = v_dataset_id and kind_code = "
        "'CROSS_CURRENCY_BASIS'\n"
        "    ) then\n"
        "        insert into ores_dq_conventions_artefact_tbl (\n"
        "            dataset_id, tenant_id, kind_code, name, oresmd_uri\n"
        "        ) values\n"
        "            (v_dataset_id, v_tenant_id, 'CROSS_CURRENCY_BASIS', "
        "'XCCY', null);\n"
        "    end if;\n"
        "\n    -- CrossCurrencyBasis leg: 1 rows\n"));
    CHECK(sql.contains("            (v_dataset_id, v_tenant_id, 'XCCY', "
        "'CROSS_CURRENCY_BASIS', 'O''NIA', null);\n"));
    CHECK_FALSE(sql.contains("unused_tbl"));
    CHECK(sql.contains("            (v_dataset_id, v_tenant_id, 'EUR-DEP', 1, "
        "v_party_id, 'EUR-EURIBOR-3M', '2', '1e-05', 'ACT/365.FIXED', "
        "'oresmd://euribor/3m');\n"));
    CHECK(sql.contains("            dataset_id, tenant_id, id, version, eom, "
        "oresmd_uri\n"));
    CHECK(sql.contains("            (v_dataset_id, v_tenant_id, "
        "'EUR-EURIBOR-3M', 1, 'true', 'oresmd://euribor/3m'),\n"
        "            (v_dataset_id, v_tenant_id, 'O''NIA', 1, null, "
        "'oresmd://onia');\n"));
    CHECK(sql.contains("    -- TenorBasisSwap pay receive: 1 rows\n"));
    CHECK(sql.contains("'TB-1', 'TENOR_BASIS_SWAP', 'EUR-EURIBOR-3M', "
        "'false');\n"));
    CHECK_FALSE(sql.contains("EUR-USD"));
    CHECK(sql.find("-- CrossCurrencyBasis central") <
        sql.find("-- Deposit:"));
    CHECK(sql.find("-- IborIndex:") < sql.find("-- TenorBasisSwap central"));
    CHECK(sql.ends_with("    end if;\nend $$;\n"));
}

TEST_CASE("the script's errors write nothing", "[codecs][conventions_dataset]") {
    auto unknown = tree();
    unknown[here + "conventions.tsv"] += "Planted\tP\tX=1\tx\t\t\n";
    CHECK(tests::generate(codecs::conventions_dataset, unknown).empty());
    auto unmapped = tree();
    unmapped[here + "conventions.tsv"] +=
        "Deposit\tD2\tIndex=NOWHERE\tx\t\t\n";
    CHECK(tests::generate(codecs::conventions_dataset, unmapped).empty());
    auto normaliser = tree();
    normaliser[here + "conventions.tsv"] +=
        "Deposit\tD3\tDayCounter=Planted\tx\t\t\n";
    CHECK(tests::generate(codecs::conventions_dataset, normaliser).empty());
    auto stated = tree();
    stated[here + "conventions.tsv"] +=
        "Deposit\tD4\tSpotDays=1\tx\toresmd://d4\t\n";
    CHECK(tests::generate(codecs::conventions_dataset, stated).empty());
}

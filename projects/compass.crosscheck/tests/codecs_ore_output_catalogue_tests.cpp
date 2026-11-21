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
#include "compass.crosscheck/codecs/ore_output_catalogue.hpp"

#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/data_tables.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;
namespace cops = compass::codec::ops;

const std::string data_path =
    "projects/ores.ore/core/src/results/output_catalogue_data.cpp";
const std::string page_path = "doc/knowledge/external/ore_output_formats.org";
const std::string files_path = "projects/ores.sql/utility/ore_files.py";
const std::string document = "compass/checks/ore_output_catalogue.org";

/**
 * @brief A table of the check document, its rows a line apart from line
 * @p line.
 */
cops::data_table table(std::string name, std::uint32_t line,
    std::vector<std::string> columns,
    std::vector<std::vector<std::string>> rows) {
    cops::data_table t{.name = std::move(name), .line = line,
        .columns = std::move(columns), .rows = std::move(rows)};
    for (std::size_t r = 0; r < t.rows.size(); ++r)
        t.row_lines.push_back(line + 2 + static_cast<std::uint32_t>(r));
    return t;
}

/**
 * @brief The tables the check document gives the codec over the fixture.
 * They are the subset of ORE Studio's tables the fixture's samples meet,
 * and the descriptions of its five families.
 */
cops::data_tables curation() {
    cops::data_tables t{.source = document};
    t.tables = {
        table("Family conventions", 10, {"family", "analytic"},
            {{"exposure-trade", "xva"}, {"bacva", "bacva"}}),
        table("Numeric qualifiers", 20, {"column"}, {{"DateIndex"}}),
        table("Family roles", 30, {"family", "role"},
            {{"scenariodump", "risk_factors"}, {"marketdata", "notebook"},
             {"counterparties", "legacy"}}),
        table("Aliases", 40, {"family", "name"},
            {{"flows", "cashflow.csv"}, {"stresstest", "stress.csv"}}),
        table("Same report", 50, {"sample", "family"},
            {{"stress", "stresstest"}}),
        table("Out of scope", 60, {"file", "reason"},
            {{"cube.csv.gz", "a compressed copy of the NPV cube"}}),
        table("Kind overrides", 70, {"family", "column", "kind"},
            {{"crif", "Bucket", "text"}}),
        table("Keys", 80, {"column", "family"},
            {{"TradeId", ""}, {"Date", ""}, {"Scenario", ""},
             {"Counterparty", "counterparties"}}),
        table("Prefixes", 90, {"prefix", "family"},
            {{"exposure_trade_", "exposure-trade"}}),
        table("Nulls", 100, {"value"}, {{""}, {"#N/A"}}),
        table("Family documentation", 110, {"family", "description", "units"},
            {{"npv_report", "The value of each trade.",
              "Money. Notional | too."},
             {"cashflow_report", "The flows.", "Money."},
             {"counterparties", "The counterparties.", "None."},
             {"trade_exposure_report", "The exposure.", "Money in base."},
             {"stress_test_report", "The stress.", "Money."}})};
    return t;
}

tests::options data(const cops::data_tables& t = curation()) {
    return {{std::string(cops::data_option), cops::encode_data_tables(t)}};
}

/**
 * @brief The table @p name of @p t.
 */
cops::data_table& in(cops::data_tables& t, std::string_view name) {
    for (auto& table : t.tables)
        if (table.name == name)
            return table;
    FAIL("no table " << name);
    return t.tables.front();
}

// generate_output_catalogue.py was run on this fixture without clang-format;
// the expected texts are what it wrote.
const std::string file1 = R"f(#TradeId,NPV,Date
T1,1.5,2020-01-01
T2,#N/A,2020-01-02
)f";

const std::string file2 = R"f(TradeId,NPV,Notional
T3,"3",x

T4,"4,5","q""uote"
)f";

const std::string file3 = R"f(TradeId,Scenario,Base NPV
T1,s1,1
T1,s2,2
)f";

const std::string file4 = R"f(#TradeId,Date,EPE
T1,2020-01-01,1e3
)f";

const std::string file5 = R"f(Counterparty,Rating
C1,AA
)f";

const std::string file6 = R"f(Ignored,Columns
1,2
)f";

const std::string file7 = R"f(Notebook,Ignored
)f";

const std::string file8 = R"f(TradeId,Amount|Base
T1,10
)f";

const std::string file9 = R"f(Not,Read
)f";

const std::string file10 = R"f(    ('npv.outputFileName', 'npv', 'outputFileName', 'output', 'x', 'y', null, 'npv.csv', 3, 4,
    ('stress.outputFile', 'stress', 'outputFile', 'output', 'x', 'y', 'z', 'stresstest.csv', 1, 1,
    ('xva.cube', 'xva', 'cubeFile', 'output', 'x', 'y', null, 'cube.csv.gz', 1, 1,
)f";

const std::string file11 = R"f(import re

P = "produced"

SCHEMAS = [
    ("master_input_schema", "Analytics Schema", "ore.xsd"),
]

RESULT_ARCHIVE = "[0-9a-f]{8}\\.tar\\.gz"

FILES = [
    entry("npv_report", "NPV", "valuation_report", P, "csv", "npv.csv",
          section="sec:npv"),
    entry("cashflow_report", "Flows", "valuation_report", P, "csv", "flows.csv",
          "cashflow.csv"),
    entry("counterparties", "Counterparties", "portfolio_details", P, "csv",
          "counterparties.csv"),
    entry("trade_exposure_report", "Exposure", "xva_report", P, "csv",
          "exposure_trade_*.csv"),
    entry("stress_test_report", "Stress", "risk_report", P, "csv", "stresstest.csv"),
    entry("archive", "Archive", "packaging", P, "tar", "result.tar.gz",
          patterns=(RESULT_ARCHIVE,)),
]
)f";

const std::string expected_records = R"CATX(const std::string_view output_catalogue_text{
R"CAT(
counterparties	counterparties.csv			Counterparty	Counterparty,Rating	Rating		0	1	external/ore/examples/Legacy/Old/ExpectedOutput/counterparties.csv		0	Counterparty:text,Rating:text
trade_exposure_report	exposure_trade_*.csv		xva	TradeId,Date	Date,EPE,TradeId		EPE	0	1	external/ore/examples/SuiteB/ExpectedOutput/exposure_trade_T1.csv		0	TradeId:text,Date:date,EPE:decimal
cashflow_report	flows.csv			TradeId	Amount|Base,TradeId		Amount|Base	0	1	projects/ores.ore/data/samples/synthetic/flows.csv	cashflow.csv	1	TradeId:text,Amount|Base:integer
npv_report	npv.csv	npv.outputFileName	npv	TradeId	NPV,TradeId	NPV,Date,Notional		0	2	external/ore/examples/SuiteA/ExpectedOutput/npv.csv		0	TradeId:text,NPV:text,Date:date,Notional:text
stress_test_report	stresstest.csv	stress.outputFile	stress	TradeId,Scenario	Base NPV,Scenario,TradeId		Base NPV	0	1	external/ore/examples/SuiteA/ExpectedOutput/stress.csv	stress.csv	0	TradeId:text,Scenario:text,Base NPV:integer
)CAT"
};

}
)CATX";

const std::string expected_families = R"o(** Families

*** counterparties

The counterparties.

| Field | Value |
|-------+-------|
| File | =counterparties.csv= |
| Parameter | none; the engine names it |
| Analytics | none stated |
| Key | Counterparty |
| Measures | closed |
| Units | None. |
| Header variants | 1 |
| Sample | =external/ore/examples/Legacy/Old/ExpectedOutput/counterparties.csv= |

| Column | Kind | Role |
|--------+------+------|
| Counterparty | text | key |
| Rating | text | qualifier |

*** trade_exposure_report

The exposure.

| Field | Value |
|-------+-------|
| File | =exposure_trade_*.csv= |
| Parameter | none; the engine names it |
| Analytics | xva |
| Key | TradeId, Date |
| Measures | closed |
| Units | Money in base. |
| Header variants | 1 |
| Sample | =external/ore/examples/SuiteB/ExpectedOutput/exposure_trade_T1.csv= |

| Column | Kind | Role |
|--------+------+------|
| TradeId | text | key |
| Date | date | key |
| EPE | decimal | measure |

*** cashflow_report

The flows.

| Field | Value |
|-------+-------|
| File | =flows.csv= |
| Other names | =cashflow.csv= |
| Parameter | none; the engine names it |
| Analytics | none stated |
| Key | TradeId |
| Measures | closed |
| Units | Money. |
| Header variants | 1 |
| Sample | =projects/ores.ore/data/samples/synthetic/flows.csv= (synthetic) |

| Column | Kind | Role |
|--------+------+------|
| TradeId | text | key |
| Amount\vert{}Base | integer | measure |

*** npv_report

The value of each trade.

| Field | Value |
|-------+-------|
| File | =npv.csv= |
| Parameter | =npv.outputFileName= |
| Analytics | npv |
| Key | TradeId |
| Measures | closed |
| Units | Money. Notional \vert{} too. |
| Header variants | 2 |
| Sample | =external/ore/examples/SuiteA/ExpectedOutput/npv.csv= |

| Column | Kind | Role |
|--------+------+------|
| TradeId | text | key |
| NPV | text | qualifier |
| Date | date | qualifier |
| Notional | text | qualifier |

*** stress_test_report

The stress.

| Field | Value |
|-------+-------|
| File | =stresstest.csv= |
| Other names | =stress.csv= |
| Parameter | =stress.outputFile= |
| Analytics | stress |
| Key | TradeId, Scenario |
| Measures | closed |
| Units | Money. |
| Header variants | 1 |
| Sample | =external/ore/examples/SuiteA/ExpectedOutput/stress.csv= |

| Column | Kind | Role |
|--------+------+------|
| TradeId | text | key |
| Scenario | text | key |
| Base NPV | integer | measure |

* See also

- [[id:8BF75172-CD91-461A-A3BA-2B4E913D63A8][ORE output processing]] — how the files are read, and why their shape follows the configuration.
- [[id:D697A04C-387C-451E-AC90-1989FCDFCAB9][Report post-processing]] — which store takes which shape.
- [[id:969C14D2-4280-4D7C-8F51-1D395B521C83][Journey: read what a run produced]] — the reader's view of the same files.
- [[id:CF2C4F3A-9C5C-4E7A-9E16-2A3A8C1D4C11][External knowledge]]
)o";


files tree() {
    return {
        {"external/ore/examples/SuiteA/ExpectedOutput/npv.csv", file1},
        {"external/ore/examples/SuiteA/ExpectedOutput/sub/npv_2.csv", file2},
        {"external/ore/examples/SuiteA/ExpectedOutput/stress.csv", file3},
        {"external/ore/examples/SuiteB/ExpectedOutput/exposure_trade_T1.csv", file4},
        {"external/ore/examples/Legacy/Old/ExpectedOutput/counterparties.csv", file5},
        {"external/ore/examples/Legacy/Old/ExpectedOutput/npv.csv", file6},
        {"external/ore/examples/ORE-Python/Notebooks/Ex1/ExpectedOutput/npv.csv", file7},
        {"projects/ores.ore/data/samples/synthetic/flows.csv", file8},
        {"projects/ores.ore/data/samples/captured/npv.csv", file9},
        {"projects/ores.sql/populate/reporting/reporting_file_types_populate.sql", file10},
        {"projects/ores.sql/utility/ore_files.py", file11}};
}

void replace(std::string& text, const std::string& from,
    const std::string& to) {
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
}

}

TEST_CASE("each family's columns, keys, kinds and sample are the ones the "
    "samples state, in the script's order", "[codecs][ore_output_catalogue]") {
    const auto out = tests::generate(codecs::ore_output_catalogue, tree(),
        data());
    REQUIRE(out.size() == 3);
    CHECK(out.at(data_path).ends_with(expected_records));
    CHECK(out.at(page_path).ends_with(expected_families));
}

TEST_CASE("a family the docs do not describe, or a seed output no family "
    "reads, writes nothing and says why", "[codecs][ore_output_catalogue]") {
    auto t = curation();
    auto& docs = in(t, "Family documentation");
    docs.rows.erase(docs.rows.begin() + 2);
    docs.row_lines.pop_back();
    CHECK(tests::generate(codecs::ore_output_catalogue, tree(), data(t))
        .empty());
    CHECK(tests::messages_of(tests::run(codecs::ore_output_catalogue, tree(),
        data(t))) == std::vector<std::string>{data_path + " is not "
            "generated: the family counterparties has no row in the table "
            "Family documentation of " + document});

    auto seed = tree();
    seed.at("projects/ores.sql/populate/reporting/"
        "reporting_file_types_populate.sql") +=
        "    ('x.out', 'x', 'out', 'output', 'x', 'y', null, 'unread.csv', 1, 1,\n";
    CHECK(tests::generate(codecs::ore_output_catalogue, seed, data()).empty());
}

TEST_CASE("an edit to a table changes what the codec writes",
    "[codecs][ore_output_catalogue]") {
    auto t = curation();
    in(t, "Family conventions").rows[0][1] = "cva";
    auto& keys = in(t, "Keys");
    keys.rows.pop_back();
    keys.row_lines.pop_back();
    in(t, "Family documentation").rows[3][2] = "Euros.";
    const auto out = tests::generate(codecs::ore_output_catalogue, tree(),
        data(t));
    REQUIRE(out.size() == 3);
    const auto& records = out.at(data_path);
    CHECK(records.find("trade_exposure_report\texposure_trade_*.csv\t\tcva\t"
        "TradeId,Date\t") != std::string::npos);
    CHECK(records.find("counterparties\tcounterparties.csv\t\t\t\t"
        "Counterparty,Rating\tCounterparty,Rating\t") != std::string::npos);
    CHECK(out.at(page_path).find("| Units | Euros. |") != std::string::npos);
}

TEST_CASE("a codec whose check document gives no tables, or lacks one, says "
    "which, and writes nothing", "[codecs][ore_output_catalogue]") {
    const auto none = tests::run(codecs::ore_output_catalogue, tree());
    REQUIRE(none.findings.size() == 1);
    CHECK(std::get<0>(none.findings.front()) == document);
    CHECK(tests::messages_of(none).front().starts_with(
        "ore_output_catalogue reads its data from a check document, and none "
        "gives it: write compass/checks/ore_output_catalogue.org"));
    CHECK(tests::generate(codecs::ore_output_catalogue, tree()).empty());

    auto t = curation();
    std::erase_if(t.tables, [](const cops::data_table& table) {
        return table.name == "Keys";
    });
    CHECK(tests::messages_of(tests::run(codecs::ore_output_catalogue, tree(),
        data(t))) == std::vector<std::string>{document + " has no table "
            "under the level-1 heading \"* Keys\", which ore_output_catalogue "
            "reads"});
    CHECK(tests::generate(codecs::ore_output_catalogue, tree(), data(t))
        .empty());

    auto roles = curation();
    in(roles, "Family roles").rows[1][1] = "notebooks";
    CHECK(tests::messages_of(tests::run(codecs::ore_output_catalogue, tree(),
        data(roles))) == std::vector<std::string>{document + ":33: the role "
            "notebooks is none of risk_factors, notebook and legacy"});
}

TEST_CASE("a missing or unreadable ore_files.py is named, as a file "
    "compass-ng reads", "[codecs][ore_output_catalogue]") {
    auto gone = tree();
    gone.erase(files_path);
    const auto missing = tests::run(codecs::ore_output_catalogue, gone, data());
    CHECK(tests::places(missing.findings) ==
        std::vector<std::pair<std::string, std::uint32_t>>{{files_path, 0}});
    CHECK(tests::messages_of(missing) == std::vector<std::string>{
        "compass-ng's ore_output_catalogue reads this file, and it is "
        "missing"});

    auto broken = tree();
    replace(broken.at(files_path), "FILES = [", "FILES = make([");
    CHECK(tests::messages_of(tests::run(codecs::ore_output_catalogue, broken,
        data())) == std::vector<std::string>{"compass-ng's "
            "ore_output_catalogue reads SCHEMAS and FILES from this file, and "
            "they do not read as the literals it reads"});
    CHECK(tests::generate(codecs::ore_output_catalogue, broken, data())
        .empty());
}

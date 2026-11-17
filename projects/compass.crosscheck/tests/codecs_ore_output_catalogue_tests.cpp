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
#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

const std::string data_path =
    "projects/ores.ore/core/src/results/output_catalogue_data.cpp";
const std::string page_path = "doc/knowledge/external/ore_output_formats.org";

// generate_output_catalogue.py was run on this fixture without clang-format;
// the expected texts are what it wrote.
const std::string file0 = R"f(# Files the engine names by a convention of its own, with the analytic that
# writes them. A family whose name a seed parameter states takes its analytic
# from the seed instead.
CONVENTION = {
    "bacva": "bacva",
    "colva-nettingset": "xva",
    "credit-migration": "creditcurves",
    "cva_sensitivities": "xvaSensitivity",
    "dim_evolution": "xva",
    "dim_regression": "xva",
    "eigenvalues": "calibration",
    "eigenvectors": "calibration",
    "exposure-nettingset": "xva",
    "exposure-trade": "xva",
    "historical_PnL": "historicalSimulationVar",
    "im_schedule": "imschedule",
    "im_schedule_trade": "imschedule",
    "mean-reversion": "calibration",
    "pca": "calibration",
    "regulation_breakdown_simm": "simm",
    "saccr": "saccr",
    "saccr_detail": "saccr",
    "sacva": "sacva",
    "sacva_sensitivities": "sacva",
    "sacvadetail": "sacva",
    "scenario_fullSurface": "scenario",
    "scenariostatistics": "scenario",
    "sensitivity_atmOnly": "sensitivity",
    "sensitivity_fullSurface": "sensitivity",
    "sensitivity_scenario": "sensitivity",
    "simm": "simm",
    "simm_data": "simm",
    "smrc": "smrc",
    "smrcdetail": "smrc",
    "stress": "stress",
    "stress_scenario_par_rates": "stress",
    "stress_scenarios": "scenarioGeneration",
    "stresstest": "stress",
    "stresstest_flows": "stress",
    "swaption_npv": "npv",
    "todaysmarketcalibration": "todaysMarketCalibration",
    "todaysmarketcalibration_cashflows": "todaysMarketCalibration",
    "xvaExplain": "xvaExplain",
    "xvaExplain_details": "xvaExplain",
    "xva_par_sensitivity": "xvaSensitivity",
    "xva_zero_sensitivity": "xvaSensitivity",
    "xvacg-cva-sensi-scenario": "xva",
    "xvacg-exposure": "xva",
    "scenario_par_rates": "sensitivity",
    "xva": "xva",
}

# Numeric columns that state the context of a figure rather than the figure:
# the size of a shift, the time to a maturity, the index of a point.
NUMERIC_QUALIFIERS = {
    "ShiftSize_1", "ShiftSize_2", "MaturityTime", "DateIndex", "Depth", "TimeStep",
}

# The families whose figures are the risk factors the run simulated. Their
# columns come from the configuration even where the corpus holds one sample.
RISK_FACTOR_FAMILIES = {"scenariostatistics", "stress_scenarios", "scenariodump"}

# The families the suite corpus holds no sample of, read from the notebook
# examples instead. Only these are read from there, because the notebooks also
# hold copies of families the suites already state.
NOTEBOOK_FAMILIES = {"marketdata", "fixings", "dividends", "runtimes", "scenariodump",
                     "additional_results"}
NOTEBOOKS = os.path.join("ORE-Python", "Notebooks")

# The families only the legacy examples hold a sample of. Only these are read
# from there, because the legacy examples repeat the suites with older headers.
LEGACY_FAMILIES = {"timeaveragedexposure", "marketObjects", "riskFactors", "counterparties",
                   "netting_sets", "swap_indices", "trade_types", "underlying_indices"}
LEGACY = "Legacy"

# The samples ORE did not ship: files a run here wrote, and files we wrote from
# the writer's columns. A family whose only sample is synthetic is marked so.
SAMPLES = os.path.join(ROOT, "projects", "ores.ore", "data", "samples")

# The other names a family is written under. A report the run configuration
# does not name is written under the engine's own report name, and the seed
# names the initial margin files with a .txt extension.
ALIASES = {
    "flows": ["cashflow.csv"],
    "saccr_detail": ["saccrdetail.csv"],
    "dim_evolution": ["dim_evolution.txt"],
    "dim_regression": ["dim_regression*.txt"],
    "stresstest": ["stress.csv"],
}

# The samples written under another report's name. The stress analytic writes
# its report as stress.csv when the run names it so, as the intraday power
# example does; it is the stress test report, not a family of its own.
SAME_REPORT = {"stress": "stresstest"}

# The output files the seed names that are not read into rows, and why. The
# generator refuses a seed output that is neither a family nor stated here. The
# reasons that cite a version hold for that version: re-check them when the
# vendored ORE moves.
OUT_OF_SCOPE = {
    "amcBuffer.dat": "binary path data the AMC engine reads back, not a report",
    "scenariodata.dat": "binary scenario data the engine reads back, not a report",
    "cube.csv.gz": "a compressed copy of the NPV cube; rawcube states it as text",
    "cptyCube.csv.gz": "a compressed counterparty cube the engine reads back",
    "nettingSetCube.csv.gz": "a compressed copy of the netting set cube; netcube states it as text",
    "scenariodata.csv.gz": "a compressed copy of the aggregation scenario data the engine reads back",
    "crossgamma.csv": "no writer reads sensitivity.crossGammaOutputFile at v1.8.17.0",
    "productclass_mapping.csv": "no writer reads sensitivity.productClassMappingOutputFile at v1.8.17.0",
    "creditcurves.csv": "no writer reads creditcurves.outputFileName at v1.8.17.0",
    "parsensi.csv": "no writer reads sensitivity.parRateSensitivityOutputFile at v1.8.17.0",
    "scenariodata.csv": "no writer reads simulation.aggregationScenarioDataDump at v1.8.17.0",
    "xva_sensitivity_jacobi.csv": "the xvaSensitivity analytic writes its jacobi to jacobi.csv at v1.8.17.0",
    "xva_sensitivity_jacobi_inverse.csv": "the xvaSensitivity analytic writes its jacobi to jacobi_inverse.csv at v1.8.17.0",
    "pca.csv": "a stem: the analytic writes pca_<currency>.csv, the pca family",
    "meanReversion.csv": "a stem: the analytic writes meanReversion_<currency>.csv, the mean-reversion family",
}

# The kinds a sample cannot show, because every sample of the column is empty.
KIND_OVERRIDES = {
    ("crif", "Bucket"): "text",
    ("capital_crif", "Bucket"): "text",
    ("dividends", "dividendExDate"): "date",
    ("dividends", "dividendRate"): "decimal",
    ("dividends", "dividendPaymentDate"): "date",
}

# The columns that identify a row rather than measure it. A key is text
# whatever its values look like.
KEYS = [
    "TradeId", "TradeID", "NettingSet", "NettingSetId", "NettingSetID",
    "Portfolio", "PortfolioId", "PortfolioID", "Date", "Time", "Tenor",
    "PillarDate", "Scenario", "ScenarioLabel", "LegNo", "CashflowNo",
    "RiskFactor", "Factor", "RiskFactor1", "RiskFactor2", "TradeType",
    "MarketObjectType", "MarketObjectId", "PrincipalComponent", "Sample",
    "PLDate1", "PLDate2",
]

# The keys of one family only, because another family states the same name as
# a column of its own: a cashflow states the fixing date of its coupon.
FAMILY_KEYS = {
    "marketdata": ["datumDate", "datumId"],
    "fixings": ["fixingDate", "fixingId"],
    "dividends": ["dividendExDate", "equityId"],
    "jacobi": ["ParFactor", "RawFactor"],
    "jacobi_inverse": ["RawFactor(z)", "ParFactor(c)"],
    "runtimes": ["Key"],
    "additional_results": ["TradeId", "ResultId"],
    "counterparties": ["Counterparty"],
    "netting_sets": ["NettingSets"],
    "trade_types": ["TradeTypes"],
    "marketObjects": ["MarketObjectType"],
    "riskFactors": ["RiskFactorType"],
    "underlying_indices": ["AssetType"],
}

# A family whose name a parameter states is the family of that name.
PREFIXES = [
    ("colva_nettingset_", "colva-nettingset"),
    ("exposure_nettingset_", "exposure-nettingset"),
    ("exposure_trade_", "exposure-trade"),
    ("credit_migration_", "credit-migration"),
    ("meanReversion_", "mean-reversion"),
    ("xva_par_sensitivity_", "xva_par_sensitivity"),
    ("xva_zero_sensitivity_", "xva_zero_sensitivity"),
    ("pca_", "pca"),
]

# The engine writes a value it has no figure for as its null string. It is not
# a value of the column, so it is dropped before the kind is inferred.
NULLS = {"", "#N/A", "#NA", "N/A", "null", "NULL", "NaN", "-nan"}

)f";

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

const std::string file12 = R"f(MONEY = "Money."

FAMILY_DOCS = {
    "npv_report": (
        "The value of each trade.",
        MONEY + " Notional | too."),
    "cashflow_report": ("The flows.", MONEY),
    "counterparties": ("The counterparties.", "None."),
    "trade_exposure_report": ("The exposure.", "Money " "in base."),
    "stress_test_report": ("The stress.", MONEY),
}
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
        {"projects/ores.ore/scripts/generate_output_catalogue.py", file0},
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
        {"projects/ores.sql/utility/ore_files.py", file11},
        {"projects/ores.ore/scripts/output_family_docs.py", file12}};
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
    const auto out = tests::generate(codecs::ore_output_catalogue, tree());
    REQUIRE(out.size() == 3);
    CHECK(out.at(data_path).ends_with(expected_records));
    CHECK(out.at(page_path).ends_with(expected_families));
}

TEST_CASE("a family the docs do not describe, or a seed output no family "
    "reads, writes nothing", "[codecs][ore_output_catalogue]") {
    auto docs = tree();
    replace(docs.at("projects/ores.ore/scripts/output_family_docs.py"),
        "    \"counterparties\": (\"The counterparties.\", \"None.\"),\n", "");
    CHECK(tests::generate(codecs::ore_output_catalogue, docs).empty());
    auto seed = tree();
    seed.at("projects/ores.sql/populate/reporting/"
        "reporting_file_types_populate.sql") +=
        "    ('x.out', 'x', 'out', 'output', 'x', 'y', null, 'unread.csv', 1, 1,\n";
    CHECK(tests::generate(codecs::ore_output_catalogue, seed).empty());
}

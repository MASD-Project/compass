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

}

/*
 * ORE Studio's party_id_scheme description; expected values are the
 * oracle's.
 */
TEST_CASE("the description takes its comment, line and one-line forms",
    "[codegen][entity][names]") {
    codegen::tests::entity_fixture f(R"j({"description": "Reference data table defining valid party identifier scheme types.\nExamples: 'LEI', 'BIC', 'MIC', 'DUNS'.\n\nParty ID schemes are managed by the system tenant. The optional\ncoding_scheme_code field cross-references the DQ coding scheme table."})j");
    codegen::ops::format_description(f.b, f.entity);
    CHECK(f.json("description_formatted") == R"j("Reference data table defining valid party identifier scheme types.\n * Examples: 'LEI', 'BIC', 'MIC', 'DUNS'.\n *\n * Party ID schemes are managed by the system tenant. The optional\n * coding_scheme_code field cross-references the DQ coding scheme table.")j");
    CHECK(f.json("description_lines") == R"j([
  "Reference data table defining valid party identifier scheme types.",
  "Examples: 'LEI', 'BIC', 'MIC', 'DUNS'.",
  "",
  "Party ID schemes are managed by the system tenant. The optional",
  "coding_scheme_code field cross-references the DQ coding scheme table."
])j");
    CHECK(f.json("description_oneline") == R"j("Reference data table defining valid party identifier scheme types. Examples: 'LEI', 'BIC', 'MIC', 'DUNS'. Party ID schemes are managed by the system tenant. The optional coding_scheme_code field cross-references the DQ coding scheme table.")j");
}

/*
 * ORE Studio's currency_pair_convention; expected values are the
 * oracle's.
 */
TEST_CASE("a subcomponent entity derives its component and entity names",
    "[codegen][entity][names]") {
    codegen::tests::entity_fixture f(R"j({"component": "refdata",
     "subcomponent": "api", "cached_by": "refdata.client",
     "entity_singular": "currency_pair_convention",
     "entity_plural": "currency_pair_conventions",
     "entity_title": "Currency Pair Convention"})j");
    codegen::ops::derive_component_names(f.b, f.entity);
    codegen::ops::derive_entity_names(f.b, f.entity);
    CHECK(f.json("component_upper") == "\"REFDATA\"");
    CHECK(f.json("component_include") == "\"refdata.api\"");
    CHECK(f.json("component_dir") == "\"ores.refdata/api\"");
    CHECK(f.json("component_core") == "\"refdata.core\"");
    CHECK(f.json("component_core_dir") == "\"ores.refdata/core\"");
    CHECK(f.json("component_service_dir") == "\"ores.refdata/service\"");
    CHECK(f.json("component_include_upper") == "\"REFDATA_API\"");
    CHECK(f.json("component_service_upper") == "\"REFDATA_SERVICE\"");
    CHECK(f.json("component_is_database") == "false");
    CHECK(f.json("cache_component") == "\"refdata\"");
    CHECK(f.json("cache_subcomponent") == "\"client\"");
    CHECK(f.json("cache_subcomponent_upper") == "\"CLIENT\"");
    CHECK(f.json("entity_pascal") == "\"CurrencyPairConvention\"");
    CHECK(f.json("entity_pascal_short") == "\"Convention\"");
    CHECK(f.json("entity_pascal_short_plural") == "\"Conventions\"");
    CHECK(f.json("entity_singular_upper") == "\"CURRENCY_PAIR_CONVENTION\"");
    CHECK(f.json("entity_plural_words_cap") ==
        "\"Currency Pair Conventions\"");
    CHECK(f.json("entity_title_lower") == "\"currency pair convention\"");
}

/*
 * ORE Studio's country: an irregular plural; no cached_by.
 */
TEST_CASE("the short plural follows the plural, not the singular",
    "[codegen][entity][names]") {
    codegen::tests::entity_fixture f(R"j({"component": "refdata",
     "entity_singular": "country", "entity_plural": "countries"})j");
    codegen::ops::derive_component_names(f.b, f.entity);
    codegen::ops::derive_entity_names(f.b, f.entity);
    CHECK(f.json("entity_pascal_short_plural") == "\"Countries\"");
    CHECK(f.json("cache_subcomponent") == "\"core\"");
    CHECK(f.json("component_include") == "\"refdata\"");
    CHECK(f.json("component_dir") == "\"ores.refdata\"");
}

TEST_CASE("title case capitalises after digits as Python's title does",
    "[codegen][entity][names]") {
    codegen::tests::entity_fixture f(R"j({"entity_plural": "fx_2d_rates"})j");
    codegen::ops::derive_entity_names(f.b, f.entity);
    CHECK(f.json("entity_plural_words_cap") == "\"Fx 2D Rates\"");
}

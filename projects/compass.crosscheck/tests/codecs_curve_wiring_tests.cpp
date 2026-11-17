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
#include "compass.crosscheck/codecs/curve_sections_wiring.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.crosscheck/ops/curve_wiring.hpp"
#include "curve_fixture.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;
using tests::curve_projection;
using tests::curve_schema;

const std::string module_path =
    "projects/ores.web/packages/bff/src/curve-sections.generated.ts";
const std::string parts_path = "projects/ores.sql/create/refdata/"
    "refdata_curve_configuration_parts_validate_fn_create.sql";
const std::string map_path = "projects/ores.sql/populate/refdata/"
    "refdata_curve_configuration_columns_populate.sql";
const std::string guard_path = "projects/ores.sql/create/refdata/"
    "refdata_curve_configuration_columns_validate_create.sql";
const std::string drop_path = "projects/ores.sql/drop/refdata/"
    "refdata_curve_configuration_columns_validate_drop.sql";

const std::string sections_tail = R"ts(export const CURVE_SECTIONS: readonly CurveSectionWiring[] = [
    {
        section: 'YieldCurves',
        entryElement: 'YieldCurve',
        definitionKey: 'curve_definition_id',
        entities: [
            {
                entity: 'yield_curve',
                vector: 'yield_curves',
            },
        ],
        parameters: [
            {
                entity: 'curve_definition',
                column: 'curve_id',
                ore: 'CurveId',
                xpath: 'CurveConfiguration/YieldCurves/YieldCurve/CurveId',
            },
            {
                entity: 'curve_definition',
                column: 'description',
                ore: 'CurveDescription',
                xpath: 'CurveConfiguration/YieldCurves/YieldCurve/CurveDescription',
            },
            {
                entity: 'yield_curve',
                column: 'currency',
                ore: 'Currency',
                xpath: 'CurveConfiguration/YieldCurves/YieldCurve/Currency',
            },
            {
                entity: 'yield_curve',
                column: 'day_counter',
                ore: 'DayCounter',
                xpath: 'CurveConfiguration/YieldCurves/YieldCurve/DayCounter',
            },
        ],
        blocks: [
            {
                element: 'Pillars',
                presence: 'has_pillars',
                presenceEntity: '',
            },
            {
                element: 'Segments',
                presence: '',
                presenceEntity: '',
            },
        ],
        collections: [
            {
                element: 'Segments',
                entity: 'curve_segment',
                vector: 'curve_segments',
                parentKey: 'curve_definition_id',
                kindColumn: 'segment_type',
                quoteParent: 'segment_id',
                partParent: 'segment_id',
                kinds: [
                    {
                        kind: 'Simple',
                        leaves: [
                            {
                                column: 'convention',
                                ore: 'Conventions',
                                entity: 'curve_segment_simple',
                                vector: 'segment_simples',
                            },
                            {
                                column: 'projection',
                                ore: 'ProjectionCurve',
                                entity: 'curve_segment_simple',
                                vector: 'segment_simples',
                            },
                        ],
                    },
                    {
                        kind: 'Direct',
                        leaves: [
                            {
                                column: 'convention',
                                ore: 'Conventions',
                                entity: 'curve_segment_simple',
                                vector: 'segment_simples',
                            },
                            {
                                column: 'projection',
                                ore: 'ProjectionCurve',
                                entity: 'curve_segment_simple',
                                vector: 'segment_simples',
                            },
                        ],
                    },
                ],
            },
        ],
    },
    {
        section: 'DefaultCurves',
        entryElement: 'DefaultCurve',
        definitionKey: 'curve_definition_id',
        entities: [
            {
                entity: 'default_curve',
                vector: 'default_curves',
            },
            {
                entity: 'default_curve_spec',
                vector: 'default_curve_specs',
            },
        ],
        parameters: [
            {
                entity: 'curve_definition',
                column: 'curve_id',
                ore: 'CurveId',
                xpath: 'CurveConfiguration/DefaultCurves/DefaultCurve/CurveId',
            },
            {
                entity: 'curve_definition',
                column: 'description',
                ore: 'CurveDescription',
                xpath: 'CurveConfiguration/DefaultCurves/DefaultCurve/CurveDescription',
            },
            {
                entity: 'default_curve',
                column: 'currency',
                ore: 'Currency',
                xpath: 'CurveConfiguration/DefaultCurves/DefaultCurve/Currency',
            },
        ],
        blocks: [
            {
                element: 'Configuration',
                presence: '',
                presenceEntity: 'default_curve_spec',
            },
        ],
        collections: [],
    },
];

/** One section's wiring, by the section's element name, or undefined for none. */
export function curveSectionOf(section: string): CurveSectionWiring | undefined {
    return CURVE_SECTIONS.find((candidate) => candidate.section === section);
}
)ts";

files tree() {
    return {
        {"projects/ores.ore/core/src/domain/curve_configuration_projection.cpp",
            curve_projection},
        {"external/ore/xsd/curveconfig.xsd", curve_schema},
        {"projects/ores.refdata/modeling/ores.refdata.yield_curve.org",
            tests::yield_curve_model},
        {"projects/ores.refdata/modeling/ores.refdata.default_curve.org",
            tests::default_curve_model}};
}

void replace(std::string& text, const std::string& from,
    const std::string& to) {
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
}

}

TEST_CASE("the wiring names each section's columns, blocks, segment kinds "
    "and parts as the script writes them", "[codecs][curve_sections_wiring]") {
    const auto out = tests::generate(codecs::curve_sections_wiring, tree());
    REQUIRE(out.size() == 2);
    CHECK(out.at(module_path).ends_with(sections_tail));
    CHECK(out.at(parts_path).ends_with(R"(
create or replace function ores_refdata_validate_default_curve_spec_fn(
    p_tenant_id uuid,
    p_value uuid
) returns uuid as $$
    select ores_refdata_validate_default_curve_part_fn(
        p_tenant_id, p_value, 'default_curve_spec',
        array[
            'Fitted', 'Spread', 'Benchmark'
        ]);
$$ language sql stable;

create or replace function ores_refdata_validate_curve_segment_simple_fn(
    p_tenant_id uuid,
    p_value uuid
) returns uuid as $$
    select ores_refdata_validate_curve_segment_part_fn(
        p_tenant_id, p_value, 'curve_segment_simple',
        array[
            'Simple', 'Direct'
        ]);
$$ language sql stable;
)"));
}

TEST_CASE("a column whose leaf the schema does not declare writes nothing",
    "[codecs][curve_sections_wiring]") {
    auto t = tree();
    replace(t.at("external/ore/xsd/curveconfig.xsd"),
        "<xs:element name=\"DayCounter\" type=\"xs:string\"/>", "");
    CHECK(tests::generate(codecs::curve_sections_wiring, t).empty());
    CHECK(tests::generate(codecs::curve_column_guards, t).empty());
}

TEST_CASE("sections the projection and the schema order differently write "
    "nothing", "[codecs][curve_sections_wiring]") {
    auto t = tree();
    replace(t.at("projects/ores.ore/core/src/domain/"
        "curve_configuration_projection.cpp"),
        "        {yield_section, \"YieldCurve\"},\n"
        "        {default_section, \"DefaultCurve\"},\n",
        "        {default_section, \"DefaultCurve\"},\n"
        "        {yield_section, \"YieldCurve\"},\n");
    CHECK(tests::generate(codecs::curve_sections_wiring, t).empty());
}

TEST_CASE("the column map holds each family column in code order, with the "
    "table its model names", "[codecs][curve_column_guards]") {
    const auto out = tests::generate(codecs::curve_column_guards, tree());
    REQUIRE(out.size() == 3);
    CHECK(out.at(map_path).find(") values\n"
        "    ('default_curve|currency', ores_utility_system_tenant_id_fn(), 0, "
        "'default_curve', 'currency', 'ores_refdata_default_curves_tbl', "
        "'curveconfig.xsd', '/CurveConfiguration/DefaultCurves/DefaultCurve/"
        "Currency', 'ores_refdata_service', 'system.initial_load', 'Seed the "
        "curve configuration columns'),\n"
        "    ('yield_curve|currency', ores_utility_system_tenant_id_fn(), 0, "
        "'yield_curve', 'currency', 'ores_refdata_yield_curves_tbl', "
        "'curveconfig.xsd', '/CurveConfiguration/YieldCurves/YieldCurve/"
        "Currency', 'ores_refdata_service', 'system.initial_load', 'Seed the "
        "curve configuration columns'),\n"
        "    ('yield_curve|day_counter', ores_utility_system_tenant_id_fn(), 0, "
        "'yield_curve', 'day_counter', 'ores_refdata_yield_curves_tbl', "
        "'curveconfig.xsd', '/CurveConfiguration/YieldCurves/YieldCurve/"
        "DayCounter', 'ores_refdata_service', 'system.initial_load', 'Seed the "
        "curve configuration columns')\non conflict") != std::string::npos);
    CHECK(out.at(guard_path).ends_with(R"(begin
    execute format(
        'create or replace trigger %I before insert or update on %I '
        'for each row execute function ores_refdata_validate_curve_values_fn(%L)',
        'ores_refdata_curve_values_trg', 'ores_refdata_default_curves_tbl', 'default_curve');

    execute format(
        'create or replace trigger %I before insert or update on %I '
        'for each row execute function ores_refdata_validate_curve_values_fn(%L)',
        'ores_refdata_curve_values_trg', 'ores_refdata_yield_curves_tbl', 'yield_curve');
end
$$;
)"));
    CHECK(out.at(guard_path).find("v_stored jsonb := '{}'::jsonb;") !=
        std::string::npos);
    CHECK(out.at(drop_path).ends_with(" */\n\n"
        "drop trigger if exists ores_refdata_curve_values_trg on "
        "ores_refdata_default_curves_tbl;\n"
        "drop trigger if exists ores_refdata_curve_values_trg on "
        "ores_refdata_yield_curves_tbl;\n"
        "drop function if exists ores_refdata_validate_curve_values_fn();\n"));
}

TEST_CASE("two entities on one table, or an entity with no model, write no "
    "guard", "[codecs][curve_column_guards]") {
    auto shared = tree();
    shared.at("projects/ores.refdata/modeling/ores.refdata.default_curve.org") =
        ":tablename: ores_refdata_yield_curves_tbl\n";
    CHECK(tests::generate(codecs::curve_column_guards, shared).empty());
    auto missing = tree();
    missing.erase("projects/ores.refdata/modeling/ores.refdata.yield_curve.org");
    CHECK(tests::generate(codecs::curve_column_guards, missing).empty());
}

TEST_CASE("a list is filled to 80 columns as textwrap fills it",
    "[codecs][curve_sections_wiring]") {
    std::string items;
    for (int i = 0; i < 12; ++i)
        items += (i ? ", 'Alpha" : "'Alpha") + std::to_string(i) + "'";
    CHECK(ops::curve::fill(items, 80, "            ") ==
        "            'Alpha0', 'Alpha1', 'Alpha2', 'Alpha3', 'Alpha4', "
        "'Alpha5',\n"
        "            'Alpha6', 'Alpha7', 'Alpha8', 'Alpha9', 'Alpha10', "
        "'Alpha11'");
    CHECK(ops::curve::fill("", 80, "    ").empty());
}

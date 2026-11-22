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
#include "value_sets_data.hpp"

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
    files out{
        {"projects/ores.ore/core/src/domain/curve_configuration_projection.cpp",
            curve_projection},
        {"external/ore/xsd/curveconfig.xsd", curve_schema},
        {"projects/ores.refdata/modeling/ores.refdata.yield_curve.org",
            tests::yield_curve_model},
        {"projects/ores.refdata/modeling/ores.refdata.default_curve.org",
            tests::default_curve_model}};
    out.merge(tests::value_set_models());
    return out;
}

files generate(const files& t,
    const tests::value_sets_data& d = tests::value_sets_data{}) {
    return tests::generate(codecs::curve_sections_wiring, t, d.settings());
}

tests::outcome run(const files& t) {
    return tests::run(codecs::curve_sections_wiring, t,
        tests::value_sets_data{}.settings());
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
    const auto out = generate(tree());
    REQUIRE(out.size() == 7);
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
    CHECK(generate(t).empty());
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
    CHECK(generate(t).empty());
}

TEST_CASE("the column map holds each family column in code order, with the "
    "table its model names", "[codecs][curve_sections_wiring]") {
    const auto out = generate(tree());
    REQUIRE(out.size() == 7);
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
    "guard, and say so", "[codecs][curve_sections_wiring]") {
    auto shared = tree();
    shared.at("projects/ores.refdata/modeling/ores.refdata.default_curve.org") =
        ":tablename: ores_refdata_yield_curves_tbl\n";
    auto missing = tree();
    missing.erase("projects/ores.refdata/modeling/ores.refdata.yield_curve.org");
    for (const auto& t : {shared, missing}) {
        const auto out = generate(t);
        CHECK(out.size() == 4);
        CHECK(out.contains(module_path));
        CHECK_FALSE(out.contains(map_path));
        const auto r = run(t);
        REQUIRE(r.findings.size() == 1);
        CHECK(std::get<0>(r.findings[0]) == map_path);
    }
}

TEST_CASE("the guard reads the tables and calls the functions the value "
    "set models name, and without them is not written",
    "[codecs][curve_sections_wiring]") {
    auto t = tree();
    std::erase_if(t, [](const auto& f) {
        return f.first.find("/modeling/ores.reporting.ore_") !=
            std::string::npos;
    });
    t.merge(tests::value_set_models("ore", ""));
    tests::value_sets_data d;
    d["Value set entities"] = tests::value_set_entities("");
    const auto guard = generate(t, d).at(guard_path);
    CHECK(guard.find("          from ores_ore_element_map_columns_tbl k\n"
        "          join ores_ore_parameter_vocabularies_tbl m\n") !=
        std::string::npos);
    CHECK(guard.find("if not ores_ore_value_in_set_fn(v_column.value_set_code"
        ", v_value) then") != std::string::npos);
    CHECK(guard.find("v_canonical := ores_ore_canonical_value_fn(v_column."
        "value_set_code, v_value);") != std::string::npos);
    CHECK(guard.find("ores_reporting") == std::string::npos);

    const auto out = tests::generate(codecs::curve_sections_wiring, t);
    CHECK_FALSE(out.contains(guard_path));
    CHECK(out.contains(map_path));
    const auto r = tests::run(codecs::curve_sections_wiring, t);
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<0>(r.findings[0]) == guard_path);
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

TEST_CASE("the gateway's wiring is the same table in C++, a row too wide "
    "for one line broken a value a line", "[codecs][curve_sections_wiring]") {
    const auto out = generate(tree());
    const auto& header = out.at("projects/ores.http/refdata/include/"
        "ores.http/routes/refdata/curve_section_wiring.hpp");
    CHECK(header.starts_with("/* -*- mode: c++;"));
    CHECK(header.ends_with("\n#endif\n"));
    const auto& source = out.at("projects/ores.http/refdata/src/routes/"
        "refdata/curve_section_wiring.cpp");
    CHECK(source.contains("constexpr std::array<curve_entity, 1> "
        "curve_entities_0{{\n    {\"yield_curve\", \"yield_curves\"},\n}};\n"));
    CHECK(source.contains("    {\"curve_definition\",\n"
        "     \"description\",\n     \"CurveDescription\",\n"
        "     \"CurveConfiguration/YieldCurves/YieldCurve/"
        "CurveDescription\"},\n"));
    CHECK(source.contains("constexpr std::array<curve_section, 2> "
        "curve_section_table{{\n"));
    CHECK(source.ends_with("            return &section;\n"
        "    return nullptr;\n}\n\n}\n"));
}

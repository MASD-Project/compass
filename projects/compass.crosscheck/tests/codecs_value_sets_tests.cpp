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
#include "compass.crosscheck/codecs/value_sets.hpp"

#include <algorithm>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.crosscheck/ops/json_value.hpp"
#include "compass.crosscheck/ops/ore_schema.hpp"
#include "compass.crosscheck/ops/value_sets.hpp"
#include "curve_fixture.hpp"
#include "test_checkout.hpp"
#include "value_sets_data.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

const std::string xs = R"(xmlns:xs="http://www.w3.org/2001/XMLSchema")";

bool has(const std::vector<std::string>& messages, std::string_view text) {
    return std::ranges::any_of(messages, [&](const std::string& m) {
        return m.find(text) != std::string::npos;
    });
}

const std::string curation =
    "projects/ores.seeder/datasets/ore_pricing_engines/curation/";
const std::string utility = "projects/ores.sql/utility/";

const std::string provenance = R"("reason": "r", "source": "authored",
    "source_ref": "x", "curator": "c", "date": "2026-11-17")";

/**
 * @brief The tables of the smallest check document the value sets resolve
 * with: text for a term without a list, one override, the curation's keys
 * and two sources, the curve flag set and the schema's boolean.
 */
tests::value_sets_data data() {
    tests::value_sets_data d;
    d["Kinds without a list"] = {{"text", "text"}};
    d["Type overrides"] = {{"", "SettlementDays", "integer",
        "The schema says so."}};
    d["Curation keys"] = {{"link"}, {"reason"}, {"source"}, {"source_ref"},
        {"curator"}, {"date"}};
    d["Curation sources"] = {{"ore_guide"}, {"ore_code"}};
    d["Type sets"] = {{"curve", "boolean", "curve.flag"}};
    d["Schema type sets"] = {{"boolean", "boolean"}};
    d["Schema kinds"] = {{"xs:boolean", "boolean"}};
    d["Item words"] = {{"text", ""}};
    d["Schema words"] = {{"boolean", "Flag", "true or false"}};
    return d;
}

files generate(const files& t, const tests::value_sets_data& d = data()) {
    return tests::generate(codecs::ore_value_sets, t, d.settings());
}

tests::outcome run(const files& t, const tests::value_sets_data& d = data()) {
    return tests::run(codecs::parameter_value_sets, t, d.settings());
}

/**
 * @brief The smallest tree the value sets resolve over: one global
 * parameter, linked to one enumerated set the guide states, and no
 * convention, curve or engine run settings terms.
 */
files tree() {
    files t;
    t[utility + "ore_renderings.json"] =
        R"({"tables": {"a": {}, "b": {}, "c": {}, "d": {}, "e": {}}})";
    t[utility + "convention_terms.json"] = R"({"parsers": {}, "conventions": {}})";
    t[utility + "curve_terms.json"] = R"({"parsers": {}, "sections": {}})";
    t[utility + "ore_parameter_vocabularies.json"] = "{}";
    t[utility + "curve_value_set_curation.json"] = "{}";
    t["external/ore/xsd/curveconfig.xsd"] = tests::curve_schema;
    t["projects/ores.ore/core/src/domain/curve_configuration_projection.cpp"] =
        tests::curve_projection;
    t["projects/ores.refdata/modeling/ores.refdata.yield_curve.org"] =
        tests::yield_curve_model;
    t["projects/ores.refdata/modeling/ores.refdata.default_curve.org"] =
        tests::default_curve_model;
    t["projects/ores.seeder/datasets/ore_pricing_engines/pricing_vocabulary.json"] = "{}";
    t[curation + "settings-code.json"] = "{}";
    t[curation + "globals.json"] = R"({"Mode": {}})";
    t[curation + "value-sets.json"] = R"({
  "catalogues": {},
  "sets": {
    "text": {"name": "Text", "description": "Any text.", "source_kind": "text"},
    "mode": {"name": "Mode", "description": "It's a mode.",
             "source_kind": "enumerated", "minimum": 1.0,
             "values": [
               {"value": "A", "label": "A", "description": "", "order": 0},
               {"value": "B", "label": "B", "description": "b", "order": 1,
                "is_active": false, "canonical_value": "A"}]}
  },
  "links": {"GlobalParameters|Mode": "mode"}
})";
    t[curation + "value-sets-code.json"] = R"({"sets": {}, "links": {}})";
    t[curation + "value-sets-review.json"] = "{" + provenance + "}";
    t[curation + "value-sets-guide-enums.json"] = "{" + provenance + "}";
    t[curation + "engine-settings-groups.json"] =
        R"({"value_sets": {}, "groups": []})";
    return t;
}

TEST_CASE("the seed states each value as the generator's q() does",
    "[value_sets]") {
    const auto out = generate(tree());
    const auto seed = out.find(ops::value_sets::seed_path);
    REQUIRE(seed != out.end());
    // A float bound keeps its point, a quote is doubled, an empty
    // description is null and a default is stated.
    CHECK(seed->second.find("('mode', ores_utility_system_tenant_id_fn(), 0, "
        "'Mode', 'It''s a mode.', 'enumerated', 'one', null, 1.0, null, null, "
        "false,") != std::string::npos);
    CHECK(seed->second.find("('mode:A', ores_utility_system_tenant_id_fn(), 0, "
        "'mode', 'A', 'A', null, 0, true, null,") != std::string::npos);
    CHECK(seed->second.find("('mode:B', ores_utility_system_tenant_id_fn(), 0, "
        "'mode', 'B', 'B', 'b', 1, false, 'A',") != std::string::npos);
    CHECK(seed->second.find("('GlobalParameters:Mode', "
        "ores_utility_system_tenant_id_fn(), 0, 'GlobalParameters', 'Mode', "
        "'mode',") != std::string::npos);
    // The set no link names is dropped, as load() drops it.
    CHECK(seed->second.find("('text',") == std::string::npos);
}

TEST_CASE("a parameter with no set is reported and stops the seed",
    "[value_sets]") {
    auto t = tree();
    t[curation + "globals.json"] = R"({"Mode": {}, "Depth": {}})";
    const auto found = tests::messages_of(run(t));
    CHECK(has(found, "GlobalParameters|Depth has no value set"));
    CHECK(generate(t).empty());
}

TEST_CASE("a set's own rules are one list, the generator's and the check's",
    "[value_sets]") {
    auto t = tree();
    t[curation + "value-sets.json"] = R"({
  "catalogues": {},
  "sets": {
    "mode": {"name": "Mode", "description": "d", "source_kind": "enumerated",
             "minimum": 5, "maximum": 1,
             "values": [
               {"value": "A", "label": "A", "description": "a", "order": 0},
               {"value": "A", "label": "A", "description": "a", "order": 1,
                "canonical_value": "Z"}]}
  },
  "links": {"GlobalParameters|Mode": "mode"}
})";
    const auto found = tests::messages_of(run(t));
    CHECK(has(found, "mode: a floor above its ceiling"));
    CHECK(has(found, "mode: enumerated with a value stated twice"));
    CHECK(has(found, "mode: A names Z as its canonical form"));
}

TEST_CASE("a review file without its provenance stops the resolution",
    "[value_sets]") {
    auto t = tree();
    t[curation + "value-sets-review.json"] = R"({"reason": "r"})";
    const auto found = tests::messages_of(run(t));
    CHECK(has(found, "value-sets-review.json has no source, source_ref, "
        "curator, date"));
    CHECK(generate(t).empty());
}

TEST_CASE("an engine run setting draws its set, and one that states a "
    "control beside it stops", "[value_sets]") {
    auto t = tree();
    t[curation + "engine-settings-groups.json"] = R"({"value_sets": {
  "setup.flag": {"name": "Flag", "description": "d", "source_kind": "boolean",
    )" + provenance + R"(}},
  "groups": [{"code": "g", "name": "G", "description": "d",
    "settings": [{"column": "c", "ore": "flagIt", "value_set": "setup.flag"}]}]})";
    const auto out = generate(t);
    REQUIRE(out.contains(std::string(ops::value_sets::seed_path)));
    CHECK(out.at(std::string(ops::value_sets::seed_path)).find(
        "'Setup:flagIt', ores_utility_system_tenant_id_fn(), 0, 'Setup', "
        "'flagIt', 'setup.flag'") != std::string::npos);
    auto bad = t;
    auto& groups = bad[curation + "engine-settings-groups.json"];
    groups.replace(groups.find("\"value_set\": \"setup.flag\""), 0,
        "\"options\": [\"a\"], ");
    CHECK(has(tests::messages_of(run(bad)),
        "the setting flagIt states the control options beside its set"));
}

TEST_CASE("a per-pair control in a guarded table is reported",
    "[value_sets]") {
    auto t = tree();
    t["projects/ores.analytics/modeling/ores.analytics.pricing_pair_setting.org"] =
        "* Columns\n** kind\n";
    const auto found = tests::messages_of(run(t));
    CHECK(has(found, "pricing_pair_setting.org: states the override column kind"));
}

TEST_CASE("the schema walk reaches a leaf under nested groups and an inline "
    "type", "[ore_schema]") {
    const std::string schema = "<xs:schema " + xs + R"(>
  <xs:element name="Root" type="root"/>
  <xs:complexType name="root">
    <xs:sequence>
      <xs:choice>
        <xs:element name="Deep" type="flag"/>
      </xs:choice>
      <xs:element name="Block">
        <xs:complexType><xs:sequence>
          <xs:element name="Inner" type="xs:boolean"/>
        </xs:sequence></xs:complexType>
      </xs:element>
    </xs:sequence>
  </xs:complexType>
  <xs:simpleType name="flag">
    <xs:restriction base="xs:string"><xs:pattern value="[YN]"/></xs:restriction>
  </xs:simpleType>
</xs:schema>)";
    const ops::ore_schema::schemas s({{"a.xsd", schema}},
        {{"xs:boolean", "boolean"}});
    // The script stops one level short of a choice inside a sequence.
    const auto deep = s.rule("a.xsd", "/Root/Deep");
    REQUIRE(deep);
    CHECK(deep->first == "regex");
    CHECK(deep->second == "flag");
    const auto inner = s.rule("a.xsd", "/Root/Block/Inner");
    REQUIRE(inner);
    CHECK(inner->first == "boolean");
    const auto flag = s.restriction_of("flag");
    REQUIRE(flag);
    CHECK(flag->type.pattern == "[YN]");
    CHECK(flag->type.line == 15);
    CHECK_FALSE(s.rule("a.xsd", "/Root/Missing"));
}

TEST_CASE("a simple type two schemas declare is the first file's",
    "[ore_schema]") {
    const auto type = [&](std::string_view value) {
        return "<xs:schema " + xs + "><xs:simpleType name=\"t\"><xs:restriction "
            "base=\"xs:string\"><xs:enumeration value=\"" + std::string(value) +
            "\"/></xs:restriction></xs:simpleType></xs:schema>";
    };
    const ops::ore_schema::schemas s({{"a.xsd", type("first")},
        {"b.xsd", type("second")}}, {});
    const auto t = s.restriction_of("t");
    REQUIRE(t);
    CHECK(t->kind == "enumerated");
    CHECK(t->type.values == std::vector<std::string>{"first"});
}

TEST_CASE("JSON keeps an int apart from a float, and the last of a "
    "repeated key", "[json_value]") {
    const auto v = ops::read_json_value(
        R"({"a": 1, "b": 1.0, "c": 1e-5, "a": "é😀", "d": null})");
    REQUIRE(v);
    CHECK(ops::python_str(*v->get("b")) == "1.0");
    CHECK(ops::python_str(*v->get("c")) == "1e-05");
    CHECK(v->get("a")->str() == "\xC3\xA9\xF0\x9F\x98\x80");
    CHECK(v->keys.size() == 4);
    CHECK(ops::python_str(*v->get("d")) == "None");
    CHECK_FALSE(ops::read_json_value("{\"a\": 1,}"));
}

TEST_CASE("no check document, or one without a table, is one finding of "
    "the gate and a refusal of the generator, and the guarded files are "
    "still checked", "[value_sets]") {
    auto t = tree();
    t["projects/ores.analytics/modeling/ores.analytics.pricing_pair_setting.org"] =
        "* Columns\n** kind\n";
    const auto none = tests::run(codecs::parameter_value_sets, t);
    REQUIRE_FALSE(none.findings.empty());
    CHECK(none.findings[0] == tests::finding{
        "compass/checks/parameter_value_sets.org", 0,
        std::get<2>(none.findings[0])});
    CHECK(std::get<2>(none.findings[0]).starts_with("parameter_value_sets "
        "reads its data from a check document, and none gives it"));
    CHECK(has(tests::messages_of(none), "states the override column kind"));

    const auto lacking = tests::run(codecs::parameter_value_sets, tree(),
        data().without("List items"));
    REQUIRE_FALSE(lacking.findings.empty());
    CHECK(lacking.findings.front() == tests::finding{
        "compass/checks/value_sets.org", 0, "compass/checks/value_sets.org "
        "has no table under the level-1 heading \"* List items\", which "
        "parameter_value_sets reads"});
    CHECK(std::ranges::count_if(lacking.findings, [](const auto& f) {
        return std::get<0>(f).starts_with("compass/");
    }) == 1);

    const auto refused = tests::run(codecs::ore_value_sets, tree(),
        data().without("Schema words"));
    CHECK(tests::messages_of(refused).size() == 2);
    CHECK(has(tests::messages_of(refused), "has no table under the level-1 "
        "heading \"* Schema words\", which ore_value_sets reads"));
    CHECK(generate(tree(), [] {
        auto d = data();
        d.tables.pop_back();
        return d;
    }()).empty());
}

TEST_CASE("a cell the tables cannot hold is named with its line",
    "[value_sets]") {
    auto d = data();
    d["Unread leaves allowed"] = {{"none"}};
    CHECK(tests::messages_of(run(tree(), d)).front() ==
        "compass/checks/value_sets.org:32: the count 'none' is not a whole "
        "number, in the table \"Unread leaves allowed\" parameter_value_sets "
        "reads");
    d = data();
    d["Period pattern"].push_back({"^x$"});
    CHECK(has(tests::messages_of(run(tree(), d)), "2 rows state what one row "
        "states, in the table \"Period pattern\""));
}

TEST_CASE("an edit to a table changes what the codecs resolve",
    "[value_sets]") {
    auto t = tree();
    t[utility + "curve_value_set_curation.json"] = R"({"*|Mode": {
    "link": "parseMode", )" + provenance + "}}";
    CHECK(has(tests::messages_of(run(t)), "curation entry *|Mode has an "
        "unknown source authored"));
    auto d = data();
    d["Curation sources"].push_back({"authored"});
    const auto found = tests::messages_of(run(t, d));
    CHECK_FALSE(has(found, "unknown source"));
    CHECK(has(found, "curation entry *|Mode matches no curve leaf"));
}

}

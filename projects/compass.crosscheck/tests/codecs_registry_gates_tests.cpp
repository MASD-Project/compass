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
#include "compass.crosscheck/codecs/model_drift.hpp"
#include "compass.crosscheck/codecs/registry_gates.hpp"
#include "compass.crosscheck/codecs/template_sections.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

}

TEST_CASE("an HTTP area outside the published ones is reported",
    "[codecs][http_areas]") {
    const files t{
        {"build/scripts/route_classification.py",
         "NO_AREA = \"(none)\"\n\nAREAS = (\n    \"access\",  # people\n"
         "    \"trading\",\n    'market-data',\n    NO_AREA_SPELLED := 1,\n)\n"},
        {"projects/ores.a/modeling/a.org", "#+http_area: trading\n"}};
    CHECK(tests::messages_of(tests::run(codecs::http_areas, t)) ==
        std::vector<std::string>{"AREAS or NO_AREA is not a literal this "
            "check can read"});
    const files u{
        {"build/scripts/route_classification.py",
         "NO_AREA = \"(none)\"\n\nAREAS = (\n    \"access\",  # people\n"
         "    \"(none)\",\n    'market-data',\n)\n"},
        {"projects/ores.a/modeling/a.org",
         "#+http_area: access\n#+http_area: (none)\n#+http_area: gone \n"},
        {"projects/ores.a/modeling/deep/b.org", "#+http_area: gone\n"}};
    CHECK(tests::places(tests::run(codecs::http_areas, u).findings) ==
        places{{"projects/ores.a/modeling/a.org", 2},
            {"projects/ores.a/modeling/a.org", 3}});
}

TEST_CASE("a route unit a model opts into is registered once",
    "[codecs][route_units]") {
    const files t{
        {"projects/ores.http/refdata/src/routes/refdata/refdata_routes.cpp",
         "void register_all() {\n    currency_routes::register_routes(r);\n"
         "    currency_routes::register_routes(r);\n"
         "    ghost_routes::register_routes(r);\n"
         "    // party_routes::register_routes(r);\n}\n"},
        {"projects/ores.http/refdata/src/routes/refdata/currency_routes.cpp",
         ""},
        {"projects/ores.http/iam/src/other.cpp", ""},
        {"projects/ores.refdata/modeling/ores.refdata.currency.org",
         "#+entity_singular: currency\n:ores.cpp.http-route.enabled: true\n"},
        {"projects/ores.refdata/modeling/ores.refdata.party.org",
         "#+name_singular: party\n:ores.cpp.http-route.enabled: true\n"},
        {"projects/ores.refdata/modeling/ores.refdata.book.org",
         "#+entity_singular: book\n:ores.cpp.http-route.enabled: false\n"}};
    CHECK(tests::messages_of(tests::run(codecs::route_units, t)) ==
        std::vector<std::string>{
            "never registers party_routes, which "
            "projects/ores.refdata/modeling/ores.refdata.party.org opts into; "
            "the gateway answers nothing for it and nothing reports it",
            "registers currency_routes 2 times; the second registration "
            "claims routes the first stated",
            "registers ghost_routes, which has no unit in this component"});
}

TEST_CASE("an accepted exception must be justified",
    "[codecs][registry_exceptions]") {
    const files t{
        {"doc/knowledge/architecture/component_clean_standard.org",
         "| B01 | All | item | evidence |\n| V08 | x | y | z |\n"},
        {"projects/ores.codegen/scripts/component_registry.py",
         "from dataclasses import dataclass\n\n@dataclass(frozen=True)\n"
         "class AcceptedException:\n    item: str\n    reason: str\n"
         "    accepted_by: str\n    accepted_on: str\n\n"
         "ACCEPTED_EXCEPTIONS: dict[str, tuple] = {\n"
         "    \"iam\": (\n        AcceptedException(\n"
         "            item=\"V08\",\n            reason=(\n"
         "                \"Covered end to end, \"\n"
         "                \"not directly.\"\n            ),\n"
         "            accepted_by=\"marco\",\n"
         "            accepted_on=\"2026-09-26\",\n        ),\n"
         "        AcceptedException(item=\"V08\", reason=\" \", "
         "accepted_by=\"\", accepted_on=\"Sept\"),\n"
         "        AcceptedException(item=\"Z99\", reason=\"r\", "
         "accepted_by=\"m\", accepted_on=\"2026-01-01\"),\n    ),\n"
         "    \"ghost\": (),\n}\n\n"
         "COMPONENTS_UNDER_TEST = (\"iam\", \"refdata\")\n"}};
    CHECK(tests::messages_of(tests::run(codecs::registry_exceptions, t)) ==
        std::vector<std::string>{
            "ghost: carries accepted exceptions but is not listed in "
            "COMPONENTS_UNDER_TEST; an exception only means something for a "
            "component the gates check",
            "iam V08: recorded twice", "iam V08: states no reason",
            "iam V08: names nobody as having accepted it",
            "iam V08: accepted_on is 'Sept', not an ISO date",
            "iam Z99: not an item the standard defines"});
}

TEST_CASE("a model may not contradict the profile it binds to",
    "[codecs][model_drift]") {
    const files t{
        {"projects/modeling/variability_uuid_lookup.org",
         "#+type: profile\n* Assignments\n| Feature | Value |\n|---+---|\n"
         "| [[id:X][has_tenant_id]] | true |\n| has_pagination | false |\n"
         "| parent_entity_singular | required |\n* Notes\n| other | x |\n"},
        {"projects/modeling/variability_paged.org",
         "#+type: profile\n* Assignments\n| has_pagination | true |\n"},
        {"projects/ores.a/modeling/ores.a.good.org",
         "* Flags\n:PROPERTIES:\n:profile: uuid-lookup\n"
         ":has_tenant_id: true\n:parent_entity_singular: book\n:END:\n"},
        {"projects/ores.a/modeling/ores.a.bad.org",
         "* Flags\n:PROPERTIES:\n:profile: uuid-lookup, paged, nope\n"
         ":has_tenant_id: false\n:END:\n* C++\n** Presentation\n"
         ":PROPERTIES:\n:has_pagination: true\n:END:\n"
         "** Other\n:PROPERTIES:\n:has_pagination: false\n:END:\n"}};
    CHECK(tests::messages_of(tests::run(codecs::model_drift, t)) ==
        std::vector<std::string>{
            "profile 'uuid-lookup' fixes has_tenant_id=true, model declares "
            "false",
            "states has_pagination twice with different values (C++ / "
            "Presentation=true; C++ / Other=false)",
            "profile 'uuid-lookup' requires parent_entity_singular to be "
            "set, model states none",
            "profiles 'uuid-lookup' and 'paged' disagree on has_pagination "
            "(false vs true)",
            "binds to unknown profile 'nope'"});
}

TEST_CASE("a line chaining the tags of a section that spans lines is "
    "reported", "[codecs][template_sections]") {
    const files t{
        {"projects/ores.codegen/library/templates/a.hpp.mustache",
         "{{#a}}{{#b}}\nbody\n{{/b}}{{/a}}\n"
         "{{#c}}inline{{/c}}\n"
         "{{#d}}{{^e}}x\n{{/e}}{{/d}}\n"
         "  {{#f}} {{#g}}  \n{{/g}}\n{{/f}}\n"},
        {"projects/ores.codegen/library/templates/sub/b.mustache",
         "{{#a}}{{#b}}\n{{/b}}{{/a}}\n"}};
    CHECK(tests::places(tests::run(codecs::template_sections, t).findings) ==
        places{{"projects/ores.codegen/library/templates/a.hpp.mustache", 1},
            {"projects/ores.codegen/library/templates/a.hpp.mustache", 3},
            {"projects/ores.codegen/library/templates/a.hpp.mustache", 6},
            {"projects/ores.codegen/library/templates/a.hpp.mustache", 7}});
}

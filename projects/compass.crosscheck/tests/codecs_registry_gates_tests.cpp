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
#include "compass.crosscheck/codecs/registry_gates.hpp"
#include "compass.crosscheck/codecs/template_sections.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

}

namespace {

const tests::options standard{{"standard", "STANDARD"}};

files registry_tree() {
    return {
        {"doc/knowledge/architecture/component_clean_standard.org",
         ":PROPERTIES:\n:ID: STANDARD\n:END:\n"
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
}

}

TEST_CASE("an accepted exception must be justified",
    "[codecs][registry_exceptions]") {
    CHECK(tests::messages_of(tests::run(codecs::registry_exceptions,
        registry_tree(), standard)) ==
        std::vector<std::string>{
            "ghost: carries accepted exceptions but is not listed in "
            "COMPONENTS_UNDER_TEST; an exception only means something for a "
            "component the gates check",
            "iam V08: recorded twice", "iam V08: states no reason",
            "iam V08: names nobody as having accepted it",
            "iam V08: accepted_on is 'Sept', not an ISO date",
            "iam Z99: not an item the standard defines"});
}

TEST_CASE("the standard renamed under its ID still defines the items",
    "[codecs][registry_exceptions]") {
    auto t = registry_tree();
    const std::string old_path =
        "doc/knowledge/architecture/component_clean_standard.org";
    t["doc/knowledge/architecture/the_clean_standard.org"] = t.at(old_path);
    t.erase(old_path);
    CHECK(tests::messages_of(tests::run(codecs::registry_exceptions, t,
        standard)) == tests::messages_of(tests::run(
            codecs::registry_exceptions, registry_tree(), standard)));
    CHECK(tests::messages_of(tests::run(codecs::registry_exceptions, t,
        standard)).size() == 6);
}

TEST_CASE("a standard option with no document of its ID is reported",
    "[codecs][registry_exceptions]") {
    CHECK(tests::run(codecs::registry_exceptions, registry_tree(),
        {{"standard", "GONE"}}).findings == std::vector<tests::finding>{
            {".compass.org", 0, "registry_exceptions: no document has the "
                ":ID: GONE that the option 'standard' names"}});
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

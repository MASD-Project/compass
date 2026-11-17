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
#include "compass.docs/codecs/component_docs.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_tree.hpp"

namespace {

using namespace compass::docs;
using tests::files;

const std::string complete =
    ":PROPERTIES:\n:ID: 1B649514-55E4-4015-B71A-62BA58BEFA26\n:END:\n"
    "#+type: component\n#+description: A component.\n"
    "* Summary\n* Inputs\n* Outputs\n* Entry points\n* Dependencies\n"
    "* See also\n";

}

TEST_CASE("a complete component, simple or composite, passes",
    "[codecs][component_docs]") {
    const files t{
        {"projects/ores.a/modeling/component_overview.org", complete},
        {"projects/ores.a/modeling/a.puml", "@startuml\n"},
        {"projects/ores.a/src/a.cpp", ""},
        {"projects/ores.b/modeling/index.org", "group page\n"},
        {"projects/ores.b/core/modeling/component_overview.org", complete},
        {"projects/ores.b/core/modeling/core.puml", ""},
        {"projects/ores.b/tests/x.cpp", ""}};
    CHECK(tests::run(codecs::component_docs, t).empty());
}

TEST_CASE("each gap of a component's documentation is reported, as org "
    "parses the overview and the pages",
    "[codecs][component_docs]") {
    const files t{
        {"projects/ores.a/modeling/component_overview.org",
         "#+TYPE: ores.codegen.component\n#+description:   \n"
         "* Summary\n* Inputs \n"},
        {"projects/ores.a/modeling/diagrams/a.puml", ""},
        {"projects/ores.b/core/modeling/component_overview.org", complete},
        {"projects/ores.b/core/modeling/core.puml", ""},
        {"projects/ores.b/api/include/x.hpp", ""},
        {"projects/ores.b/api/src/x.cpp", ""},
        {"projects/ores.b/docs/readme.org", ""},
        {"projects/ores.c/modeling/notes.org", ""},
        {"projects/ores.b.extra/modeling/component_overview.org", complete},
        {"projects/ores.b.extra/modeling/e.puml", ""},
        {"doc/page.org",
         "Use `[[nodiscard]]` here, `=[[glued]]=` there, `a =[[ok]]= b`, "
         "`[[id:x]]` too.\n"
         "#+begin_src cpp\n`[[maybe_unused]]`\n#+end_src\n"}};
    CHECK(tests::messages(tests::run(codecs::component_docs, t)) ==
        std::vector<std::string>{
            "ores.a: no :ID: UUID in :PROPERTIES: block",
            "ores.a: #+description: missing or empty",
            "ores.a: missing section(s): Outputs, Entry points, "
            "Dependencies, See also",
            "ores.a: no .puml file in modeling/",
            "ores.b.api: modeling/component_overview.org not found",
            "ores.b.api: no .puml diagram in modeling/",
            "ores.c: modeling/component_overview.org not found",
            "ores.b.extra: top-level sibling of component ores.b; absorb it "
            "into that component as a sub-component or rename it",
            "org parses nodiscard inside backticks as a link, and the site "
            "export aborts on the unresolved target; use =...= or a src "
            "block: `[[nodiscard]]`",
            "org parses glued inside backticks as a link, and the site "
            "export aborts on the unresolved target; use =...= or a src "
            "block: `=[[glued]]=`"});
}

TEST_CASE("the exceptions file excuses one rule for one component",
    "[codecs][component_docs]") {
    const files t{
        {"projects/ores.a/modeling/component_overview.org", complete},
        {"projects/ores.a/modeling/notes.txt", ""},
        {"projects/ores.b/modeling/component_overview.org", complete},
        {"projects/ores.codegen/docs_exceptions.txt",
         "# comment\n\nMISSING_PUML ores.a\nMISSING_ID ores.b\n"}};
    CHECK(tests::messages(tests::run(codecs::component_docs, t)) ==
        std::vector<std::string>{"ores.b: no .puml file in modeling/"});
}

TEST_CASE("the exceptions file excuses the backtick links of one page, and "
    "the pages are reported in path order", "[codecs][component_docs]") {
    const std::string link = "Use `[[nodiscard]]` here.\n";
    const files t{
        {"doc/b/page.org", link},
        {"doc/a.org", link},
        {"doc/b.org", link},
        {"projects/ores.codegen/docs_exceptions.txt",
         "BACKTICK_LINK doc/b.org\n"}};
    const auto found = tests::run(codecs::component_docs, t);
    REQUIRE(found.size() == 2);
    CHECK(std::get<0>(found[0]) == "doc/a.org");
    CHECK(std::get<0>(found[1]) == "doc/b/page.org");
}

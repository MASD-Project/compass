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
#include "compass.docs/codecs/prototypes.hpp"
#include "compass.docs/codecs/rule_identifiers.hpp"
#include "compass.docs/codecs/tree_gates.hpp"
#include "test_tree.hpp"

namespace {

using namespace compass::docs;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

}

TEST_CASE("a shell recipe must name a document that exists",
    "[codecs][shell_recipe_provenance]") {
    const files t{
        {"projects/ores.shell/scripts/library/iam/ok.ores",
         "# GENERATED from doc/recipes/iam.org \xe2\x80\x94 do not edit by "
         "hand.\nlogin\n"},
        {"projects/ores.shell/scripts/library/iam/dotted.ores",
         "# GENERATED from doc/./x/../recipes/iam.org \xe2\x80\x94 do not edit "
         "by hand.\n"},
        {"projects/ores.shell/scripts/library/iam/gone.ores",
         "# GENERATED from doc/recipes/gone.org \xe2\x80\x94 do not edit by "
         "hand.\n"},
        {"projects/ores.shell/scripts/library/iam/hyphen.ores",
         "# GENERATED from doc/recipes/iam.org - do not edit by hand.\n"},
        {"projects/ores.shell/scripts/library/top.ores", "no header\n"},
        {"doc/recipes/iam.org", "* Recipes\n"}};
    CHECK(tests::messages(tests::run(codecs::shell_recipe_provenance, t)) ==
        std::vector<std::string>{
            "source doc/./x/../recipes/iam.org does not exist",
            "source doc/recipes/gone.org does not exist",
            "header names no source document"});
}

TEST_CASE("a skill outside the compass namespace is reported",
    "[codecs][skill_namespace]") {
    const files t{
        {"doc/llm/skills/compass-review/SKILL.org", ""},
        {"doc/llm/skills/review/SKILL.org", ""},
        {"doc/llm/skills/notes/readme.org", ""},
        {"doc/llm/skills/deep/inner/SKILL.org", ""}};
    CHECK(tests::places(tests::run(codecs::skill_namespace, t)) ==
        places{{"doc/llm/skills/review/SKILL.org", 1}});
}

TEST_CASE("a pull_request or pull_request_target event of on is reported, "
    "in any form YAML states it", "[codecs][no_pr_triggers]") {
    const files t{
        {".github/workflows/a.yml",
         "on:\n  push:\n  pull_request:  \n  pull_request_review:\n"
         "#  pull_request:\n    pull_request:\n"},
        {".github/workflows/b.yaml", "on:\n  pull_request:\n"},
        {".github/workflows/sub/c.yml", "on:\n  pull_request:\n"},
        {".github/workflows/d.yml", "name: d\non: [push, pull_request]\n"},
        {".github/workflows/e.yml", "on: pull_request_target # risky\n"},
        {".github/workflows/f.yml",
         "on:\n    pull_request_target:\n      types: [opened]\n"},
        {".github/workflows/g.yml", "\"on\":\n  - push\n  - pull_request\n"},
        {".github/workflows/h.yml", "on: { pull_request: {}, push: {} }\n"},
        {".github/workflows/i.yml",
         "on:\n  pull_request_review:\njobs:\n  pull_request:\n"
         "    runs-on: x\n"}};
    CHECK(tests::places(tests::run(codecs::no_pr_triggers, t)) ==
        places{{".github/workflows/a.yml", 3},
            {".github/workflows/b.yaml", 2},
            {".github/workflows/d.yml", 2},
            {".github/workflows/e.yml", 1},
            {".github/workflows/f.yml", 2},
            {".github/workflows/g.yml", 3},
            {".github/workflows/h.yml", 1}});
    CHECK(tests::run(codecs::no_pr_triggers, {{"x.yml", ""}}).size() == 1);
}

TEST_CASE("the prototype inventory must match the tree",
    "[codecs][prototypes]") {
    const files t{
        {"doc/prototypes/index.org", "[[file:trades/][Trades]] "
         "[[file:ghost/][Ghost]]\n"},
        {"doc/prototypes/trades/index.org",
         "| [[file:blotter/][Blotter]] | [[file:blotter/review/][review]] |\n"
         "| [[file:ticket/][Ticket]] | pending |\n"
         "| [[file:gone/][Gone]] | none |\n"},
        {"doc/prototypes/trades/blotter/index.html", ""},
        {"doc/prototypes/trades/blotter/review/index.html", ""},
        {"doc/prototypes/trades/blotter/prototype.css", "shared\nextra\n"},
        {"doc/prototypes/trades/blotter/common.js", "a\n"},
        {"doc/prototypes/trades/ticket/index.html", ""},
        {"doc/prototypes/trades/ticket/prototype.css", "shared\n"},
        {"doc/prototypes/trades/ticket/common.js", "b\n"},
        {"doc/prototypes/trades/ticket/review/notes.md", ""},
        {"doc/prototypes/loose/x/index.html", ""},
        {"projects/ores.lisp/src/ores-build-site.el",
         "(ores-deploy-prototypes)\n"},
        {"projects/ores.web/packages/web/src/prototype/a.tsx", ""}};
    CHECK(tests::messages(tests::run(codecs::prototypes, t)) ==
        std::vector<std::string>{
            "doc/prototypes/loose/x/index.html is not listed on "
            "doc/prototypes/loose/index.org",
            "doc/prototypes/trades/index.org lists gone, but "
            "doc/prototypes/trades/gone/index.html does not exist",
            "doc/prototypes/loose/ holds prototypes but has no index.org: "
            "the group needs a page",
            "doc/prototypes/index.org links the group ghost, but "
            "doc/prototypes/ghost/index.org does not exist",
            "doc/prototypes/trades/ticket/review/ has no index.html: a "
            "review lists its screens on one page",
            "doc/prototypes/trades/index.org lists ticket without its "
            "review, though the review directory exists",
            "doc/prototypes/trades/ticket/common.js differs from "
            "doc/prototypes/trades/blotter/common.js: the group shares this "
            "file by copy, so the copies must be identical",
            "projects/ores.web/packages/web/src/prototype still exists: "
            "React is for the product, not for prototypes"});
}

TEST_CASE("each UX rule carries one identifier and the checklist cites "
    "them", "[codecs][rule_identifiers]") {
    const files t{
        {"doc/knowledge/ui/record_screen_standard.org",
         "* Summary\n- not a rule\n"
         "** The list\n- *RS-LIST-01* rows\n- a rule without one\n"
         "- [ ] a task, not a rule\n#+begin_example\n- inside\n"
         "#+end_example\n- *ES-LIST-01* wrong page\n"
         "** A section nobody keys\n- *RS-X-01* keyed anyway\n- plain\n"},
        {"doc/knowledge/ui/entity_screen_specification.org",
         "** The list\n- *ES-LIST-01* again\n"},
        {"doc/knowledge/ui/screen_review_checklist.org",
         "- [ ] Rows (RS-LIST-01)\n- [ ] Odd (RS-LIST-1)\n"
         "- [ ] Nothing cited\n- [ ] Unkeyed (UXL 3)\n"
         "- [ ] Lost (RS-GONE-09)\n- [x] done\n"}};
    CHECK(tests::messages(tests::run(codecs::rule_identifiers, t)) ==
        std::vector<std::string>{
            "a rule without an identifier at its start under 'The list'",
            "ES-LIST-01 carries another page's code",
            "a rule under 'A section nobody keys', a section this check does "
            "not key",
            "ES-LIST-01 is also on a rule of "
            "doc/knowledge/ui/record_screen_standard.org",
            "RS-LIST-1 is not an identifier; two digits are required",
            "cites RS-LIST-1, which no page carries",
            "a checklist line that cites no identifier",
            "cites RS-GONE-09, which no page carries"});
}

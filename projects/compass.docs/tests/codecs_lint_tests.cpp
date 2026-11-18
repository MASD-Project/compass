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
#include "compass.docs/codecs/lint.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_tree.hpp"

namespace {

using namespace compass::docs;
using tests::files;

const std::string target_id = "1B649514-55E4-4015-B71A-62BA58BEFA26";
const std::string task_id = "2C759625-66F5-5126-C82B-73CB69F26B37";

std::string page(std::string_view id, std::string_view type,
    std::string_view body) {
    return ":PROPERTIES:\n:ID: " + std::string(id) + "\n:END:\n#+type: " +
        std::string(type) + "\n" + std::string(body);
}

}

TEST_CASE("a page whose tags, markers and links are sound passes",
    "[codecs][lint]") {
    const files t{
        {"doc/a.org", page(target_id, "knowledge",
            "#+filetags: :codec:a-b.c_d:\n"
            "# BEGIN generated x (tools/gen.py)\nbody\n# END generated x\n"
            "See [[id:" + target_id + "][itself]] and [[id:UUID]].\n")},
        {"tools/gen.py", "open('doc/a.org', 'w')\n"}};
    CHECK(tests::run(codecs::lint, t).empty());
}

TEST_CASE("only the first filetags line is read, and each bad tag of it "
    "is reported", "[codecs][lint]") {
    const files t{{"doc/a.org", page(target_id, "knowledge",
        "#+filetags: :Upper:ok:: spaced:9lead:\n#+filetags: :Second:\n")}};
    CHECK(tests::messages(tests::run(codecs::lint, t)) ==
        std::vector<std::string>{"unknown/malformed tag 'Upper'",
            "unknown/malformed tag ' spaced'",
            "unknown/malformed tag '9lead'"});
}

TEST_CASE("a BEGIN marker without its END on a line of its own is "
    "reported, and a mismatched END closes nothing", "[codecs][lint]") {
    const files t{{"doc/a.org", page(target_id, "knowledge",
        "# BEGIN generated a\n# END generated b\n"
        "#+BEGIN generated c\nprose # END generated c\n"
        "# BEGIN generated d\n  # END generated d  \n"
        "#+begin_example\n# BEGIN generated e (tools/missing.py)\n"
        "#+end_example\n")}};
    const auto found = tests::run(codecs::lint, t);
    CHECK(tests::messages(found) == std::vector<std::string>{
        "'# BEGIN generated a' has no matching '# END generated a' on its "
        "own line"});
    CHECK(tests::places(found) ==
        std::vector<std::pair<std::string, std::uint32_t>>{{"doc/a.org", 5}});
}

TEST_CASE("a marker must name a script that exists and names the page",
    "[codecs][lint]") {
    const files t{
        {"doc/a.org", page(target_id, "knowledge",
            "# BEGIN generated a (tools/missing.py)\n# END generated a\n"
            "# BEGIN generated b (tools/other.py)\n# END generated b\n"
            "# BEGIN generated c ( ./tools/../tools/gen.py )\n"
            "# END generated c\n")},
        {"doc/llm/skills/my-skill/SKILL.org",
         "# BEGIN generated s (tools/gen.py)\n# END generated s\n"},
        {"tools/gen.py", "writes a.org and my-skill/\n"},
        {"tools/other.py", "writes b.org\n"}};
    CHECK(tests::messages(tests::run(codecs::lint, t)) ==
        std::vector<std::string>{
            "marker names tools/missing.py, which does not exist",
            "marker names tools/other.py, which never writes this file"});
}

TEST_CASE("a link to an ID no linted page holds dangles, and one that is "
    "not a UUID is malformed", "[codecs][lint]") {
    const files t{
        {"doc/a.org", page(target_id, "knowledge",
            "[[id:" + task_id + "]] [[id:" + target_id + "][x]]\n"
            "[[id:1B649514-55E4][short]] [[id:placeholder]] [[id:ABCDEF]]\n")},
        {"external/ore/b.org", page(task_id, "task", "")},
        {"doc/.hidden.org", page(task_id, "task", "")},
        {"projects/x/.claude/c.org", page(task_id, "task", "")}};
    CHECK(tests::messages(tests::run(codecs::lint, t)) ==
        std::vector<std::string>{
            "[[id:1B649514-55E4]] (13 chars) is not a UUID, so the page "
            "cannot export",
            "[[id:" + task_id + "]] -> (no label) resolves to no :ID: in "
            "the corpus"});
}

TEST_CASE("an ID in a heading's drawer resolves a link, in any case",
    "[codecs][lint]") {
    const files t{
        {"doc/a.org", page(target_id, "knowledge",
            "* Heading\n:PROPERTIES:\n:ID: 2c759625-66f5-5126-c82b-"
            "73cb69f26b37\n:END:\n")},
        {"doc/b.org", "[[id:" + task_id + "][upper]] "
            "[[id:1b649514-55e4-4015-b71a-62ba58befa26][lower]]\n"}};
    CHECK(tests::run(codecs::lint, t).empty());
}

TEST_CASE("an :ID: line inside a block is an example, not a page's ID",
    "[codecs][lint]") {
    const files t{
        {"doc/a.org", page(target_id, "knowledge",
            "#+begin_example\n:PROPERTIES:\n:ID: " + task_id +
            "\n:END:\n#+end_example\n"
            "  #+BEGIN_SRC org\n:ID: " + task_id + "\n  #+END_SRC\n")},
        {"doc/b.org", "[[id:" + task_id + "][example]]\n"}};
    CHECK(tests::messages(tests::run(codecs::lint, t)) ==
        std::vector<std::string>{"[[id:" + task_id + "]] -> example "
            "resolves to no :ID: in the corpus"});
}

TEST_CASE("a durable page linking an agile one is reported only with "
    "strict_links", "[codecs][lint]") {
    const files t{
        {"doc/knowledge/k.org", page(target_id, "knowledge",
            "Made by [[id:" + task_id + "][the\ntask]].\n")},
        {"doc/agile/t.org", page(task_id, "task",
            "[[id:" + target_id + "][the page]]\n")}};
    CHECK(tests::run(codecs::lint, t).empty());
    CHECK(tests::messages(tests::run(codecs::lint, t,
        {{"strict_links", "true"}})) == std::vector<std::string>{
            "doc/knowledge/k.org -> task: the task"});
}

TEST_CASE("an id link is one org parses: one whose label wraps a line "
    "dangles, and one in a block is text", "[codecs][lint]") {
    const files t{{"doc/a.org", page(target_id, "knowledge",
        "See [[id:" + task_id + "][a label that\nwraps]].\n"
        "#+begin_src text\n[[id:" + task_id + "][in a block]]\n#+end_src\n"
        "#+BEGIN_EXAMPLE\n[[id:" + task_id + "][upper]] [[id:x-y]]\n"
        "#+END_EXAMPLE\n")}};
    const auto found = tests::run(codecs::lint, t);
    CHECK(tests::messages(found) == std::vector<std::string>{
        "[[id:" + task_id + "]] -> a label that wraps resolves to no :ID: "
        "in the corpus"});
    CHECK(tests::places(found) ==
        std::vector<std::pair<std::string, std::uint32_t>>{{"doc/a.org", 5}});
}

TEST_CASE("a #+type: or #+filetags: line inside a block is not the page's",
    "[codecs][lint]") {
    const files t{
        {"doc/knowledge/k.org", page(target_id, "knowledge",
            "Made by [[id:" + task_id + "][the task]].\n")},
        {"doc/agile/t.org", ":PROPERTIES:\n:ID: " + task_id + "\n:END:\n"
            "#+begin_example\n#+type: task\n#+filetags: :Bad:\n"
            "#+end_example\n"}};
    CHECK(tests::run(codecs::lint, t, {{"strict_links", "true"}}).empty());
}

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
#include "compass.codec/codecs/required_sections.hpp"

#include <array>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;
namespace ot = compass::org::types;

types::diagnostic_table findings(const tests::test_corpus& t) {
    const std::array codecs{codecs::required_sections};
    const auto r = ops::make_registry(codecs, t.types(), {});
    return ops::run(r, t.corpus(), t.types(), 2).diagnostics[0];
}

}

TEST_CASE("a document missing a required section is reported with the "
    "heading to add", "[codecs][required_sections]") {
    tests::test_corpus t;
    t.add("complete.org", "#+type: task\n* Goal\n* Result\n");
    const std::string source = "#+title: t\n#+type:  task \n* Goal\ntext\n";
    t.add("task.org", source);

    const auto d = findings(t);

    REQUIRE(d.size() == 1);
    const types::diagnostic_id first{0};
    CHECK(d.documents[first] == ot::document_id{1});
    CHECK(d.ranges[first].of(source) == "task");
    CHECK(codecs::required_sections.rules[d.rules[first].value].name ==
        "missing_section");
    CHECK(d.levels[first] == types::severity::error);
    CHECK(d.messages[first] == "task document has no \"Result\" section");
    CHECK(d.fixes[first] ==
        "Add the heading \"* Result\"; the task contract requires it.");
}

TEST_CASE("each missing section is its own finding, in contract order",
    "[codecs][required_sections]") {
    tests::test_corpus t;
    t.add("story.org", "#+type: story\n* Background\n");

    const auto d = findings(t);

    REQUIRE(d.size() == 2);
    CHECK(d.messages[{0}] == "story document has no \"Goal\" section");
    CHECK(d.messages[{1}] == "story document has no \"Status\" section");
}

TEST_CASE("a complete document is not reported, whatever the level, "
    "keyword and tags of its headings", "[codecs][required_sections]") {
    tests::test_corpus t;
    t.add("flat.org", "#+type: task\n* Goal\n* Result\n");
    t.add("nested.org",
        "#+type: task\n* Notes\n** DONE Goal :tag:\n*** Result\n");

    CHECK(findings(t).size() == 0);
}

TEST_CASE("a section the contract calls optional is not required",
    "[codecs][required_sections]") {
    tests::test_corpus t;
    t.add("story.org", "#+type: story\n* Goal\n* Status\n");

    CHECK(findings(t).size() == 0);
}

TEST_CASE("a document with no registered type is not checked",
    "[codecs][required_sections]") {
    tests::test_corpus t;
    t.add("untyped.org", "* Anything\n");
    t.add("unknown.org", "#+type: scribble\n* Anything\n");

    CHECK(findings(t).size() == 0);
}

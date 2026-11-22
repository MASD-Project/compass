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
#include "compass.db/ops/extract.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.html/ops/render.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass::db;

types::corpus_document extracted(std::string_view source) {
    compass::org::types::tables t;
    compass::ingest::org::parse(source, t);
    return ops::extract(compass::store::ops::blob_id_of(source), source,
        compass::org::types::view(t));
}

constexpr std::string_view page = R"(:PROPERTIES:
:ID: 11111111-AAAA-4000-8000-000000000001
:END:
#+title: Search the corpus
#+description: How a person finds a page.
#+type: task
#+level: s1
#+audience: user
#+filetags: :search:docs:
#+created: 2026-11-22
#+updated: 2026-11-23
#+tag_note: first
#+tag_note: second

The preamble names [[id:22222222-BBBB-4000-8000-000000000002][the story]].

* Goal
:PROPERTIES:
:ID: GOAL-ID
:OWNER: marco
:END:

Rank sections by =ts_rank_cd=.

** TODO [#A] Measure ranking                                   :search:perf:

See [[https://example.org/rank][the ranking notes]] and https://example.org/raw.

* Goal

A second heading with the same title.
)";

}

TEST_CASE("a document's metadata becomes columns, keywords and properties",
    "[ops][extract]") {
    const auto d = extracted(page);
    const auto& m = d.document;
    CHECK(d.hash == compass::store::ops::blob_id_of(page));
    CHECK(m.org_id == "11111111-AAAA-4000-8000-000000000001");
    CHECK(m.title == "Search the corpus");
    CHECK(m.description == "How a person finds a page.");
    CHECK(m.type == "task");
    CHECK(m.level == "s1");
    CHECK(m.audience == "user");
    CHECK(m.created == "2026-11-22");
    CHECK(m.updated == "2026-11-23");
    CHECK(m.tags == std::vector<std::string>{"search", "docs"});
    CHECK(m.keywords.find(R"("tag_note":["first","second"])") !=
        std::string::npos);
    CHECK(m.properties == R"({"ID":"11111111-AAAA-4000-8000-000000000001"})");
    CHECK(m.source == page);
}

TEST_CASE("headings carry their path, state, tags and a unique anchor",
    "[ops][extract]") {
    const auto d = extracted(page);
    REQUIRE(d.headings.size() == 3);
    const auto& goal = d.headings[0];
    CHECK(goal.level == 1);
    CHECK(goal.title == "Goal");
    CHECK(goal.anchor == "GOAL-ID");
    CHECK(goal.org_id == "GOAL-ID");
    CHECK(goal.properties == R"({"ID":"GOAL-ID","OWNER":"marco"})");

    const auto& measure = d.headings[1];
    CHECK(measure.level == 2);
    CHECK(measure.title == "Measure ranking");
    CHECK(measure.path == std::vector<std::string>{"Goal", "Measure ranking"});
    CHECK(measure.todo == "TODO");
    CHECK(measure.priority == "A");
    CHECK(measure.tags == std::vector<std::string>{"search", "perf"});
    CHECK(measure.anchor == "goal-measure-ranking");

    CHECK(d.headings[2].anchor == "goal");
    CHECK_FALSE(d.headings[2].org_id);
}

TEST_CASE("sections hold the prose of each heading as plain text",
    "[ops][extract]") {
    const auto d = extracted(page);
    REQUIRE(d.sections.size() == 4);
    CHECK_FALSE(d.sections[0].heading);
    CHECK(d.sections[0].text == "The preamble names the story.");
    CHECK(d.sections[1].heading == 0u);
    CHECK(d.sections[1].text == "Rank sections by =ts_rank_cd=.");
    CHECK(d.sections[2].heading == 1u);
    CHECK(d.sections[2].text == "See the ranking notes and "
        "https://example.org/raw.");
    CHECK(d.sections[3].heading == 2u);
}

TEST_CASE("links carry their heading, target and text", "[ops][extract]") {
    const auto d = extracted(page);
    REQUIRE(d.links.size() == 3);
    CHECK_FALSE(d.links[0].heading);
    CHECK(d.links[0].type == "id");
    CHECK(d.links[0].target_id == "22222222-BBBB-4000-8000-000000000002");
    CHECK(d.links[0].text == "the story");
    CHECK(d.links[1].heading == 1u);
    CHECK(d.links[1].type == "https");
    CHECK(d.links[1].path == "//example.org/rank");
    CHECK_FALSE(d.links[1].target_id);
    CHECK(d.links[1].text == "the ranking notes");
    CHECK(d.links[2].heading == 1u);
}

TEST_CASE("a page that is not a test scenario has no test definition",
    "[ops][extract]") {
    CHECK_FALSE(extracted(page).scenario);
}

namespace {

std::string scenario(std::string_view state, std::string_view result,
    std::string_view step_two) {
    return std::string(R"(:PROPERTIES:
:ID: 33333333-CCCC-4000-8000-000000000003
:END:
#+title: Test Scenario: Watch the quotes
#+type: test_scenario
#+target_dialog:
#+environment: bright_faraday

* Scenario Info

| Field         | Value |
|---------------+-------|
| Verifies task | [[id:44444444-DDDD-4000-8000-000000000004][Build the quotes view]] |
| Parent story  | [[id:55555555-EEEE-4000-8000-000000000005][See the market]] |
| Target screen | Market data, then Quotes |
| State         | )") + std::string(state) + R"( |

* Steps

** Log in

- Sign in as the tenant administrator.

*** Result

| Field  | Value |
|--------+-------|
| Status | )" + std::string(result) + R"( |

** Open the quotes view

- )" + std::string(step_two) + R"(

* Results

| Status | )" + std::string(state) + " |\n";
}

}

TEST_CASE("a test scenario gives its links, target, environment and steps",
    "[ops][extract]") {
    const auto source = scenario("PENDING", "", "Open Market data.");
    const auto d = extracted(source);
    REQUIRE(d.scenario);
    const auto& s = *d.scenario;
    CHECK(s.org_id == "33333333-CCCC-4000-8000-000000000003");
    CHECK(s.title == "Test Scenario: Watch the quotes");
    CHECK(s.task_id == "44444444-DDDD-4000-8000-000000000004");
    CHECK(s.story_id == "55555555-EEEE-4000-8000-000000000005");
    CHECK(s.target == "Market data, then Quotes");
    CHECK(s.environment == "bright_faraday");
    REQUIRE(s.steps.size() == 2);
    CHECK(s.steps[0].title == "Log in");
    CHECK(s.steps[0].body == "- Sign in as the tenant administrator.");
    CHECK(s.steps[1].title == "Open the quotes view");
    CHECK(s.steps[1].body == "- Open Market data.");
}

TEST_CASE("a scenario's definition ignores results and changes with its "
    "steps", "[ops][extract]") {
    const auto before = extracted(scenario("PENDING", "", "Open it."));
    const auto tested = extracted(scenario("PASSED", "PASS", "Open it."));
    const auto edited = extracted(scenario("PENDING", "", "Open it twice."));
    REQUIRE(before.scenario);
    REQUIRE(tested.scenario);
    REQUIRE(edited.scenario);
    CHECK(before.hash != tested.hash);
    CHECK(before.scenario->definition == tested.scenario->definition);
    CHECK(before.scenario->definition != edited.scenario->definition);
}

TEST_CASE("every heading's anchor is the id the rendered page gives it",
    "[ops][extract]") {
    constexpr std::string_view source = R"(#+title: Anchors

* Goal
:PROPERTIES:
:ID: GOAL-ID
:END:
** TODO [#A] Measure ranking :search:
* Goal
* Goal
:PROPERTIES:
:ID: GOAL-ID
:END:
* Über [[https://example.org][the /linked/ =title=]]
*** Skipped a level
* ***
)";
    compass::org::types::tables t;
    compass::ingest::org::parse(source, t);
    const auto view = compass::org::types::view(t);
    const auto d = ops::extract(compass::store::ops::blob_id_of(source),
        source, view);
    const auto rendered_page = compass::html::ops::render(source, view,
        "a.org", {}, {});

    std::vector<std::string> rendered;
    const std::string_view body = rendered_page.body;
    for (auto at = body.find("<h"); at != std::string_view::npos;
         at = body.find("<h", at + 1)) {
        const auto tag = body.substr(at, body.find('>', at) - at);
        const auto id = tag.find(" id=\"");
        if (id == std::string_view::npos)
            continue;
        const auto begin = id + 5;
        rendered.emplace_back(tag.substr(begin, tag.find('"', begin) - begin));
    }
    std::vector<std::string> stored;
    for (const auto& h : d.headings)
        stored.push_back(h.anchor);
    CHECK(stored.size() == 7);
    CHECK(stored == rendered);
}

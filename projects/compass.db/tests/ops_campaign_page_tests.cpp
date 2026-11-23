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
#include "compass.db/ops/campaign_page.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.ingest/org/parser.hpp"
#include "compass.org/types/tables.hpp"

namespace {

using namespace compass::db;
using Catch::Matchers::ContainsSubstring;

types::exported_screenshot shot(std::string attachment,
    std::optional<std::int32_t> step, std::optional<std::string> caption) {
    types::exported_screenshot s;
    s.attachment = std::move(attachment);
    s.step_ord = step;
    s.caption = std::move(caption);
    return s;
}

types::exported_step step(std::int32_t ord, std::string title,
    std::optional<std::string> outcome, std::optional<std::string> notes) {
    types::exported_step s;
    s.ord = ord;
    s.title = std::move(title);
    s.outcome = std::move(outcome);
    s.notes = std::move(notes);
    return s;
}

types::exported_note note(std::int64_t id, std::string kind,
    std::string text) {
    types::exported_note n;
    n.id = id;
    n.kind = std::move(kind);
    n.text = std::move(text);
    n.author = "ben";
    n.created_at = "2026-11-21T10:00:00Z";
    return n;
}

/**
 * @brief A campaign with a failed run, a run in progress, a scenario only
 * a note names, an open and an addressed note, and screenshots on a step
 * and on a note.
 */
types::campaign_rows campaign() {
    types::campaign_rows c;
    c.id = 7;
    c.environment = "bright_faraday";
    c.story_id = "STORY-1";
    c.story_title = "Story: Serve the documents";
    c.story_path = "doc/agile/versions/v0/sprint_01/serve/story.org";
    types::page_ref sprint;
    sprint.org_id = "SPRINT-1";
    sprint.title = "Sprint 1";
    c.sprint = sprint;
    c.opened_at = "2026-11-20T09:00:00Z";

    types::exported_run failed;
    failed.id = 12;
    failed.tester = "Ana Lima";
    failed.started_at = "2026-11-20T09:00:00Z";
    types::run_finish f;
    f.at = "2026-11-20T09:30:00Z";
    f.outcome = "failed";
    failed.finished = f;
    failed.definition = "0123456789abcdef0123456789abcdef01234567";
    failed.steps = {step(0, "Log in", "pass", std::nullopt),
        step(1, "Open the list", "fail", "Empty | no rows\nafter reload"),
        step(2, "Close", std::nullopt, std::nullopt)};
    failed.screenshots = {shot("aaaa", 1, "The empty list")};

    types::exported_run running;
    running.id = 13;
    running.tester = "Ben";
    running.started_at = "2026-11-22T08:00:00Z";
    running.definition = "fedcba9876543210fedcba9876543210fedcba98";

    types::exported_scenario login;
    login.org_id = "SC-1";
    login.title = "Test Scenario: Log in";
    login.runs = {failed, running};
    types::exported_scenario noted;
    noted.org_id = "SC-2";
    noted.title = "Test Scenario: Untested";
    c.scenarios = {login, noted};

    auto open = note(3, "question", "Which tenant?");
    auto defect = note(2, "defect", "The list is empty.");
    defect.scenario_id = "SC-1";
    defect.step_ord = 1;
    types::note_address a;
    a.by = "dev";
    a.at = "2026-11-23T11:00:00Z";
    a.reply = "Fixed in the seed.";
    defect.addressed = a;
    defect.screenshots = {shot("bbbb", std::nullopt, std::nullopt)};
    c.notes = {defect, open};

    types::exported_image png;
    png.attachment = "aaaa";
    png.media_type = "image/png";
    png.bytes = "\x89PNG";
    types::exported_image jpeg;
    jpeg.attachment = "bbbb";
    jpeg.media_type = "image/jpeg";
    jpeg.bytes = "\xff\xd8";
    c.images = {png, jpeg};
    return c;
}

std::string page_of(const types::campaign_rows& c) {
    return ops::render_campaign(c).front().bytes;
}

/**
 * @brief Each heading of @p page as org reads it: its stars and title.
 */
std::vector<std::string> headings(const std::string& page) {
    compass::org::types::tables t;
    compass::ingest::org::parse(page, t);
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < page.size()) {
        const auto end = std::min(page.find('\n', at), page.size());
        const auto line = page.substr(at, end - at);
        if (line.starts_with("*"))
            out.push_back(line);
        at = end + 1;
    }
    REQUIRE(t.headings.size() == out.size());
    return out;
}

compass::org::types::tables parsed(const std::string& page) {
    compass::org::types::tables t;
    compass::ingest::org::parse(page, t);
    return t;
}

std::size_t table_cells(const compass::org::types::tables& t) {
    return static_cast<std::size_t>(std::ranges::count(t.objects.kinds.values,
        compass::org::types::object_kind::table_cell));
}

/**
 * @brief @p page with each zero-width space removed: the text a reader of
 * the page sees.
 */
std::string shown(std::string page) {
    const std::string zwsp = "\xE2\x80\x8B";
    for (auto at = page.find(zwsp); at != std::string::npos;
         at = page.find(zwsp, at))
        page.erase(at, zwsp.size());
    return page;
}

/**
 * @brief The description of each link of @p page as a reader sees it, in
 * order.
 */
std::vector<std::string> link_descriptions(const std::string& page) {
    const auto t = parsed(page);
    std::vector<std::string> out;
    for (const auto o : t.links.objects.values) {
        const auto r = t.objects.contents.values[o.value];
        out.push_back(shown(page.substr(r.begin, r.size())));
    }
    return out;
}

/**
 * @brief The target of each link of @p page, in order.
 */
std::vector<std::string> link_paths(const std::string& page) {
    const auto t = parsed(page);
    std::vector<std::string> out;
    for (const auto& r : t.links.paths.values)
        out.emplace_back(page.substr(r.begin, r.size()));
    return out;
}


/**
 * @brief org-unescape-code-in-string: drops one comma from each line that
 * is blanks, one or more commas, then * or #+.
 */
std::string unescape(std::string_view body) {
    std::string out;
    std::size_t at = 0;
    while (at < body.size()) {
        const auto end = std::min(body.find('\n', at), body.size());
        const auto line = body.substr(at, end - at);
        const auto blanks = std::min(line.find_first_not_of(" \t"),
            line.size());
        const auto commas = std::min(line.find_first_not_of(',', blanks),
            line.size());
        const auto rest = line.substr(commas);
        if (commas > blanks &&
            (rest.starts_with('*') || rest.starts_with("#+"))) {
            out += line.substr(0, blanks);
            out += line.substr(blanks + 1);
        } else {
            out += line;
        }
        out += '\n';
        at = end + 1;
    }
    return out;
}

}

TEST_CASE("render_campaign writes the page beside its story and each "
    "screenshot in a folder beside the page", "[ops][campaign_page]") {
    const auto files = ops::render_campaign(campaign());
    REQUIRE(files.size() == 3);
    const std::string folder = "doc/agile/versions/v0/sprint_01/serve/";
    CHECK(files[0].path == folder + "test_campaign_7.org");
    CHECK(files[1].path == folder + "test_campaign_7/aaaa.png");
    CHECK(files[1].bytes == "\x89PNG");
    CHECK(files[2].path == folder + "test_campaign_7/bbbb.jpg");
    CHECK(files[2].bytes == "\xff\xd8");
}

TEST_CASE("render_campaign gives the page the test_campaign frontmatter",
    "[ops][campaign_page]") {
    const auto page = page_of(campaign());
    const auto id = ops::derived_id(
        "compass test_campaign bright_faraday STORY-1 7");
    CHECK(page.starts_with(":PROPERTIES:\n:ID: " + id + "\n:END:\n"
        "#+title: Test Campaign: Serve the documents on bright_faraday\n"));
    CHECK_THAT(page, ContainsSubstring("#+type: test_campaign\n"
        "#+level: s1\n#+filetags: :serve:sprint_01:v0:\n"
        "#+created: 2026-11-20\n#+updated: 2026-11-23\n"
        "#+environment: bright_faraday\n#+todo: TODO | DONE\n"));
    CHECK_THAT(page, ContainsSubstring("This page records a [[id:" +
        std::string(ops::test_campaign_contract_id) + "][test campaign]] of "
        "[[id:STORY-1][Story: Serve the documents]]"));
    CHECK_THAT(page, ContainsSubstring("| Sprint      | "
        "[[id:SPRINT-1][Sprint 1]] "));
    CHECK_THAT(page, ContainsSubstring("| Runs        | 2, 1 in progress"));
    CHECK_THAT(page, ContainsSubstring("| Notes       | 1 open, 1 addressed"));
}

TEST_CASE("render_campaign writes each run under its scenario with its "
    "tester, times, definition, outcome, steps and screenshots",
    "[ops][campaign_page]") {
    const auto page = page_of(campaign());
    const auto run_id = ops::derived_id(
        "compass test_campaign bright_faraday STORY-1 7 run 12");
    CHECK_THAT(page, ContainsSubstring(
        "** [[id:SC-1][Test Scenario: Log in]]\n\n"
        "*** Run 12: failed\n:PROPERTIES:\n:ID: " + run_id + "\n:END:\n\n"
        "| Field      | Value                |\n"
        "|------------+----------------------|\n"
        "| Tester     | Ana Lima             |\n"
        "| Started    | 2026-11-20T09:00:00Z |\n"
        "| Finished   | 2026-11-20T09:30:00Z |\n"
        "| Definition | 0123456789ab         |\n"
        "| Outcome    | failed               |\n\n"
        "| Step | Title         | Outcome | Notes                              |\n"
        "|------+---------------+---------+------------------------------------|\n"
        "| 1    | Log in        | pass    |                                    |\n"
        "| 2    | Open the list | fail    | Empty \\vert{} no rows after reload |\n"
        "| 3    | Close         | -       |                                    |\n\n"
        "Screenshots:\n\n"
        "- [[file:test_campaign_7/aaaa.png]], step 2: The empty list\n"));
    CHECK_THAT(page, ContainsSubstring("*** Run 13: in progress\n"));
    CHECK_THAT(page, ContainsSubstring("| Finished   | not yet "));
    CHECK_THAT(page, ContainsSubstring(
        "** [[id:SC-2][Test Scenario: Untested]]\n\nNo runs.\n"));
}

TEST_CASE("render_campaign groups the notes by kind, TODO while open and "
    "DONE once addressed, with the reply", "[ops][campaign_page]") {
    const auto page = page_of(campaign());
    const auto h = headings(page);
    const std::vector<std::string> expected{"* Campaign", "* Scenarios",
        "** [[id:SC-1][Test Scenario: Log in]]", "*** Run 12: failed",
        "*** Run 13: in progress", "** [[id:SC-2][Test Scenario: Untested]]",
        "* Notes", "** defect",
        "*** DONE Note 2 on [[id:SC-1][Test Scenario: Log in]], step 2",
        "** question", "*** TODO Note 3 on the campaign"};
    CHECK(h == expected);
    const auto note_id = ops::derived_id(
        "compass test_campaign bright_faraday STORY-1 7 note 2");
    CHECK_THAT(page, ContainsSubstring(":ID: " + note_id + "\n"));
    CHECK_THAT(page, ContainsSubstring("| Author       | ben "));
    CHECK_THAT(page, ContainsSubstring("| Addressed by | dev "));
    CHECK_THAT(page, ContainsSubstring("#+begin_example\nThe list is empty.\n"
        "#+end_example\n\nThe reply:\n\n#+begin_example\nFixed in the seed.\n"
        "#+end_example\n\nScreenshots:\n\n"
        "- [[file:test_campaign_7/bbbb.jpg]]\n"));
}

TEST_CASE("render_campaign escapes what org would read in a note as "
    "structure", "[ops][campaign_page]") {
    auto c = campaign();
    c.notes = {note(4, "defect", "* Not a heading\n  ** Nor this\n"
        "#+end_example\n,* Already escaped once\n:PROPERTIES:\nplain")};
    c.notes[0].author = "a | b";
    const auto page = page_of(c);
    CHECK_THAT(page, ContainsSubstring("#+begin_example\n,* Not a heading\n"
        "  ,** Nor this\n,#+end_example\n,,* Already escaped once\n"
        ":PROPERTIES:\nplain\n#+end_example\n"));
    const auto t = parsed(page);
    REQUIRE(t.blocks.size() == 1);
    const auto body = t.blocks.bodies.values[0];
    CHECK(unescape(std::string_view(page).substr(body.begin, body.size())) ==
        c.notes[0].text + "\n");
    CHECK_THAT(page, ContainsSubstring("| Author  | a \\vert{} b "));
    const auto h = headings(page);
    CHECK(h.back() == "*** TODO Note 4 on the campaign");
    CHECK(h.size() == 9);
}

TEST_CASE("render_campaign keeps brackets and bars in tester text and "
    "titles from becoming links or cells", "[ops][campaign_page]") {
    const auto plain_page = page_of(campaign());
    const auto plain = parsed(plain_page);
    auto c = campaign();
    c.story_title = "Story: A | B ]] C";
    c.sprint->title = "Sprint ] 1]";
    auto& run = c.scenarios[0].runs[0];
    run.tester = "[[id:NOPE-3][ana]]";
    run.steps[0].title = "[[id:NOPE-4]] | x";
    run.steps[1].notes = "see [[id:NOPE-1][the spec]]";
    run.steps[2].notes = "plain id:NOPE-5 and file:x.org";
    run.screenshots[0].caption = "[[id:NOPE-2]] cap";
    c.scenarios[1].title = "Test Scenario: [[x]] | y]";
    c.notes[0].author = "[[file:/etc/passwd]]";
    const auto page = page_of(c);
    const auto hostile = parsed(page);
    CHECK(link_paths(page) == link_paths(plain_page));
    CHECK(table_cells(hostile) == table_cells(plain));
    CHECK(hostile.headings.size() == plain.headings.size());
    for (const auto* text : {"see [[id:NOPE-1][the spec]]",
             "[[id:NOPE-2]] cap", "[[id:NOPE-3][ana]]",
             "plain id:NOPE-5 and file:x.org"})
        CHECK_THAT(shown(page), ContainsSubstring(text));
    const auto descriptions = link_descriptions(page);
    for (const auto* title : {"Story: A \\vert{} B ]] C", "Sprint ] 1]",
             "Test Scenario: [[x]] \\vert{} y]"})
        CHECK(std::ranges::count(descriptions, std::string(title)) > 0);
}

TEST_CASE("render_campaign is a function of the rows: the same rows give "
    "the same bytes, and a new note changes only the page",
    "[ops][campaign_page]") {
    const auto before = ops::render_campaign(campaign());
    CHECK(ops::render_campaign(campaign())[0].bytes == before[0].bytes);
    auto c = campaign();
    c.notes.push_back(note(9, "question", "And another?"));
    const auto after = ops::render_campaign(c);
    REQUIRE(after.size() == before.size());
    CHECK(after[0].path == before[0].path);
    CHECK(after[0].bytes != before[0].bytes);
    for (std::size_t i = 1; i < after.size(); ++i) {
        CHECK(after[i].path == before[i].path);
        CHECK(after[i].bytes == before[i].bytes);
    }
}

TEST_CASE("derived_id is a version 5 UUID, the same for the same name and "
    "another for another", "[ops][campaign_page]") {
    const auto id = ops::derived_id("compass test_campaign main S 1");
    CHECK(std::regex_match(id, std::regex(
        "[0-9A-F]{8}-[0-9A-F]{4}-5[0-9A-F]{3}-[89AB][0-9A-F]{3}-"
        "[0-9A-F]{12}")));
    CHECK(ops::derived_id("compass test_campaign main S 1") == id);
    CHECK(ops::derived_id("compass test_campaign main S 2") != id);
}

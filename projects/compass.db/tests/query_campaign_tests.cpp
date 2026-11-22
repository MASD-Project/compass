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
#include "compass.db/query/campaign.hpp"

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.db/io/corpus.hpp"
#include "compass.db/io/schema.hpp"
#include "compass.db/ops/extract.hpp"
#include "compass.db/ops/run_outcome.hpp"
#include "compass.db/query/testing.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "database.hpp"

namespace {

using namespace compass::db;
using types::scenario_state;
using types::step_outcome;
using reason = query::rejected_write::reason;

class documents final {
public:
    types::corpus_document add(std::string source) {
        auto& text = sources_.emplace_back(std::move(source));
        auto& t = tables_.emplace_back();
        compass::ingest::org::parse(text, t);
        return ops::extract(compass::store::ops::blob_id_of(text), text,
            compass::org::types::view(t));
    }

private:
    std::deque<std::string> sources_;
    std::deque<compass::org::types::tables> tables_;
};

std::string scenario_page(std::string_view id, std::string_view story,
    std::string_view step_three = "Close the widget.") {
    return ":PROPERTIES:\n:ID: " + std::string(id) + "\n:END:\n"
        "#+title: Test Scenario: " + std::string(id) + "\n"
        "#+type: test_scenario\n\n"
        "This page documents a test scenario.\n\n* Scenario Info\n\n"
        "| Field         | Value |\n|---------------+-------|\n"
        "| Parent story  | [[id:" + std::string(story) + "][Story]] |\n\n"
        "* Steps\n\n** Log in\n\n- Sign in.\n\n** Open\n\n- Open it.\n\n"
        "** Close\n\n- " + std::string(step_three) + "\n";
}

std::vector<types::corpus_document> corpus(documents& docs,
    std::string_view step_three = "Close the widget.") {
    return {
        docs.add(":PROPERTIES:\n:ID: SPRINT-1\n:END:\n#+title: Sprint 1\n"
            "#+type: sprint\n\n* Status\n\n| Field | Value |\n"
            "|-------+-------|\n| State | STARTED |\n"),
        docs.add(":PROPERTIES:\n:ID: STORY-1\n:END:\n#+title: Story one\n"
            "#+type: story\n\nThis page documents a story in "
            "[[id:SPRINT-1][Sprint 1]].\n"),
        docs.add(":PROPERTIES:\n:ID: STORY-2\n:END:\n#+title: Story two\n"
            "#+type: story\n\nThis page documents a story in "
            "[[id:SPRINT-1][Sprint 1]].\n"),
        docs.add(scenario_page("SC-1", "STORY-1", step_three)),
        docs.add(scenario_page("SC-2", "STORY-1")),
        docs.add(scenario_page("SC-3", "STORY-2")),
        docs.add(":PROPERTIES:\n:ID: SC-LOOSE\n:END:\n#+title: Loose\n"
            "#+type: test_scenario\n\n* Steps\n\n** One\n\n- Do it.\n"),
    };
}

void place(io::connection& c, const std::string& environment,
    const std::vector<types::corpus_document>& pages) {
    std::vector<types::placement> placed;
    for (std::size_t i = 0; i < pages.size(); ++i)
        placed.push_back({"p" + std::to_string(10 + i) + ".org",
            pages[i].hash});
    types::environment e;
    e.name = environment;
    e.root = "/work/" + environment;
    io::write_environment(c, e, pages, placed);
}

io::connection fresh() {
    io::connection c(tests::fresh_database());
    io::init(c, "english");
    documents docs;
    place(c, "main", corpus(docs));
    return c;
}

const types::note_kinds kinds{{"defect", "question"}};

types::start_run_response start(io::connection& c, std::string scenario,
    std::string environment = "main", std::string tester = "ana") {
    types::start_run_request r;
    r.environment = std::move(environment);
    r.scenario_id = std::move(scenario);
    r.tester = std::move(tester);
    return query::start_run(c, r);
}

void step(io::connection& c, std::int64_t run, std::int32_t ord,
    step_outcome o, std::string tester = "ana") {
    types::step_result_request r;
    r.run = run;
    r.step_ord = ord;
    r.outcome = o;
    r.tester = std::move(tester);
    query::record_step_result(c, r);
}

types::finish_run_response finish(io::connection& c, std::int64_t run,
    std::string tester = "ana") {
    types::finish_run_request r;
    r.run = run;
    r.tester = std::move(tester);
    return query::finish_run(c, r);
}

types::add_note_request note_request(std::int64_t campaign) {
    types::add_note_request r;
    r.campaign = campaign;
    r.kind = "defect";
    r.text = "The widget squeaks.";
    r.tester = "ana";
    return r;
}

types::attach_request screenshot_of_run(std::int64_t run) {
    types::attach_request r;
    r.run = run;
    r.media_type = "image/png";
    r.bytes = std::string("\x89PNG\r\n\x1a\n\0\x01\xff", 11);
    r.tester = "ana";
    return r;
}

scenario_state state(io::connection& c, std::string_view id,
    const std::string& environment = "main") {
    types::sprint_request r;
    r.environment = environment;
    for (const auto& story : query::what_needs_testing(c,
             types::view_filter{"true"}, r).stories)
        for (const auto& s : story.scenarios)
            if (s.org_id == id)
                return s.state;
    FAIL("no scenario " << id);
    return scenario_state::untested;
}

std::string scalar(io::connection& c, const std::string& sql) {
    return std::string(c.exec(sql).value(0, 0));
}

template <typename F>
reason rejection(F&& f) {
    try {
        f();
    } catch (const query::rejected_write& e) {
        return e.why();
    }
    FAIL("the write was not rejected");
    return reason::invalid;
}

}

TEST_CASE("a run's outcome is failed, then blocked, then passed",
    "[ops][campaign]") {
    using O = std::optional<step_outcome>;
    const auto derive = [](std::vector<O> steps) {
        return ops::derive_run_outcome(steps);
    };
    CHECK(derive({step_outcome::pass, step_outcome::skipped}) ==
        types::run_outcome::passed);
    CHECK(derive({}) == types::run_outcome::passed);
    CHECK(derive({step_outcome::blocked, step_outcome::fail}) ==
        types::run_outcome::failed);
    CHECK(derive({step_outcome::blocked, std::nullopt}) ==
        types::run_outcome::blocked);
    CHECK(derive({step_outcome::fail, std::nullopt}) ==
        types::run_outcome::failed);
    const auto open = derive({step_outcome::pass, std::nullopt,
        step_outcome::skipped, std::nullopt});
    REQUIRE_FALSE(open);
    CHECK(open.error() == std::vector<std::int32_t>{1, 3});
}

TEST_CASE("the first run opens the story's campaign and follows the "
    "definition the environment holds", "[query][campaign]") {
    auto c = fresh();
    const auto first = start(c, "SC-1");
    CHECK(first.run > 0);
    CHECK_FALSE(first.started_at.empty());
    REQUIRE(first.steps.size() == 3);
    CHECK(first.steps[0].ord == 0);
    CHECK(first.steps[0].title == "Log in");
    CHECK(first.steps[2].body == "- Close the widget.");
    CHECK(first.definition == scalar(c, "SELECT encode(definition, 'hex') "
        "FROM test.scenario WHERE org_id = 'SC-1'"));
    CHECK(scalar(c, "SELECT tester || ' ' || sprint_id || ' ' || story_id "
        "FROM activity.run r JOIN activity.campaign ca ON ca.id = r.campaign "
        "WHERE r.id = " + std::to_string(first.run)) ==
        "ana SPRINT-1 STORY-1");

    CHECK(start(c, "SC-2").campaign == first.campaign);
    CHECK(start(c, "SC-1", "main", "ben").campaign == first.campaign);
    CHECK(start(c, "SC-3").campaign != first.campaign);
    CHECK(scalar(c, "SELECT count(*) FROM activity.campaign") == "2");
    CHECK(scalar(c, "SELECT count(*) FROM activity.run") == "4");
}

TEST_CASE("each recorded run moves its scenario's state",
    "[query][campaign]") {
    auto c = fresh();
    CHECK(state(c, "SC-1") == scenario_state::untested);

    const auto a = start(c, "SC-1");
    step(c, a.run, 0, step_outcome::pass);
    step(c, a.run, 1, step_outcome::skipped);
    step(c, a.run, 2, step_outcome::pass);
    CHECK(state(c, "SC-1") == scenario_state::untested);
    CHECK(finish(c, a.run).outcome == types::run_outcome::passed);
    CHECK(state(c, "SC-1") == scenario_state::passed);

    const auto b = start(c, "SC-1");
    step(c, b.run, 0, step_outcome::pass);
    step(c, b.run, 1, step_outcome::fail);
    CHECK(finish(c, b.run).outcome == types::run_outcome::failed);
    CHECK(state(c, "SC-1") == scenario_state::failed);

    const auto d = start(c, "SC-1");
    step(c, d.run, 0, step_outcome::blocked);
    CHECK(finish(c, d.run).outcome == types::run_outcome::blocked);
    CHECK(state(c, "SC-1") == scenario_state::failed);

    const auto e = start(c, "SC-1");
    step(c, e.run, 0, step_outcome::fail);
    step(c, e.run, 0, step_outcome::pass);
    step(c, e.run, 1, step_outcome::pass);
    step(c, e.run, 2, step_outcome::pass);
    CHECK(finish(c, e.run).outcome == types::run_outcome::passed);
    CHECK(state(c, "SC-1") == scenario_state::passed);
    CHECK(state(c, "SC-2") == scenario_state::untested);
}

TEST_CASE("a new definition makes a pass changed, and a run of it follows "
    "the new steps", "[query][campaign]") {
    auto c = fresh();
    const auto a = start(c, "SC-1");
    for (std::int32_t i = 0; i < 3; ++i)
        step(c, a.run, i, step_outcome::pass);
    finish(c, a.run);
    CHECK(state(c, "SC-1") == scenario_state::passed);

    const auto old = start(c, "SC-1");
    documents docs;
    place(c, "main", corpus(docs, "Close the widget and lock it."));
    CHECK(state(c, "SC-1") == scenario_state::changed);

    const auto b = start(c, "SC-1");
    CHECK(b.definition != a.definition);
    CHECK(b.steps[2].body == "- Close the widget and lock it.");
    for (std::int32_t i = 0; i < 3; ++i)
        step(c, b.run, i, step_outcome::pass);
    finish(c, b.run);
    CHECK(state(c, "SC-1") == scenario_state::passed);

    for (std::int32_t i = 0; i < 3; ++i)
        step(c, old.run, i, step_outcome::pass);
    finish(c, old.run);
    CHECK(state(c, "SC-1") == scenario_state::changed);
}

TEST_CASE("runs and notes in one environment leave another untouched",
    "[query][campaign]") {
    auto c = fresh();
    documents docs;
    place(c, "release", corpus(docs));
    const auto a = start(c, "SC-1", "release");
    step(c, a.run, 0, step_outcome::fail);
    finish(c, a.run);
    query::add_note(c, kinds, note_request(a.campaign));
    CHECK(state(c, "SC-1", "release") == scenario_state::failed);
    CHECK(state(c, "SC-1") == scenario_state::untested);
    CHECK(start(c, "SC-1").campaign != a.campaign);
    CHECK_THROWS_AS(start(c, "SC-1", "nowhere"), query::unknown_environment);
}

TEST_CASE("a write without a tester is rejected and writes nothing",
    "[query][campaign]") {
    auto c = fresh();
    CHECK(rejection([&] { start(c, "SC-1", "main", ""); }) ==
        reason::invalid);
    CHECK(rejection([&] { start(c, "SC-1", "main", " \t"); }) ==
        reason::invalid);
    CHECK(scalar(c, "SELECT count(*) FROM activity.campaign") == "0");

    const auto a = start(c, "SC-1");
    CHECK(rejection([&] { step(c, a.run, 0, step_outcome::pass, ""); }) ==
        reason::invalid);
    auto shot = screenshot_of_run(a.run);
    shot.tester = "";
    CHECK(rejection([&] { query::attach_screenshot(c, shot); }) ==
        reason::invalid);
    auto n = note_request(a.campaign);
    n.tester = " ";
    CHECK(rejection([&] { query::add_note(c, kinds, n); }) ==
        reason::invalid);
    const auto note = query::add_note(c, kinds, note_request(a.campaign));
    types::address_note_request addressed;
    addressed.note = note;
    CHECK(rejection([&] { query::address_note(c, addressed); }) ==
        reason::invalid);
    CHECK(rejection([&] { finish(c, a.run, ""); }) == reason::invalid);
    CHECK(scalar(c, "SELECT count(*) FROM activity.step_result") == "0");
    CHECK(scalar(c, "SELECT count(*) FROM activity.attachment") == "0");
    CHECK(scalar(c, "SELECT count(*) FROM activity.note") == "1");
    CHECK(scalar(c, "SELECT state FROM activity.note") == "open");
    CHECK(scalar(c, "SELECT count(*) FROM activity.run "
        "WHERE finished_at IS NOT NULL") == "0");
}

TEST_CASE("a run starts only of a placed scenario with a story",
    "[query][campaign]") {
    auto c = fresh();
    CHECK(rejection([&] { start(c, "SC-9"); }) == reason::not_found);
    CHECK(rejection([&] { start(c, "SC-LOOSE"); }) == reason::invalid);
    CHECK(scalar(c, "SELECT count(*) FROM activity.campaign") == "0");
}

TEST_CASE("a step result names a step of the run's definition, on an "
    "unfinished run", "[query][campaign]") {
    auto c = fresh();
    const auto a = start(c, "SC-1");
    CHECK(rejection([&] { step(c, a.run, 3, step_outcome::pass); }) ==
        reason::invalid);
    CHECK(rejection([&] { step(c, a.run, -1, step_outcome::pass); }) ==
        reason::invalid);
    CHECK(rejection([&] { step(c, a.run + 100, 0, step_outcome::pass); }) ==
        reason::not_found);

    types::step_result_request r;
    r.run = a.run;
    r.step_ord = 1;
    r.outcome = step_outcome::fail;
    r.notes = "The lid sticks.";
    r.tester = "ana";
    query::record_step_result(c, r);
    CHECK(scalar(c, "SELECT outcome || ': ' || notes FROM "
        "activity.step_result") == "fail: The lid sticks.");
    finish(c, a.run);
    CHECK(rejection([&] { step(c, a.run, 0, step_outcome::pass); }) ==
        reason::conflict);
}

TEST_CASE("a run finishes once, and only when its outcome is known",
    "[query][campaign]") {
    auto c = fresh();
    const auto a = start(c, "SC-1");
    step(c, a.run, 0, step_outcome::pass);
    step(c, a.run, 2, step_outcome::skipped);
    CHECK(rejection([&] { finish(c, a.run); }) == reason::invalid);
    CHECK(scalar(c, "SELECT count(*) FROM activity.run "
        "WHERE outcome IS NOT NULL") == "0");
    step(c, a.run, 1, step_outcome::pass);
    const auto done = finish(c, a.run);
    CHECK(done.outcome == types::run_outcome::passed);
    CHECK_FALSE(done.finished_at.empty());
    CHECK(rejection([&] { finish(c, a.run); }) == reason::conflict);
    CHECK(rejection([&] { finish(c, a.run + 100); }) == reason::not_found);
}

TEST_CASE("a screenshot is stored once by its content hash",
    "[query][campaign]") {
    auto c = fresh();
    const auto a = start(c, "SC-1");
    auto shot = screenshot_of_run(a.run);
    shot.step_ord = 1;
    shot.caption = "The lid";
    const auto first = query::attach_screenshot(c, shot);
    CHECK(first.attachment == compass::store::ops::to_hex(
        compass::store::ops::blob_id_of(shot.bytes)));
    CHECK(scalar(c, "SELECT encode(bytes, 'hex') FROM activity.attachment")
        == "89504e470d0a1a0a0001ff");

    shot.caption = "The lid, again";
    const auto again = query::attach_screenshot(c, shot);
    CHECK(again.attached == first.attached);
    CHECK(scalar(c, "SELECT caption FROM activity.attached") ==
        "The lid, again");

    shot.step_ord.reset();
    const auto whole_run = query::attach_screenshot(c, shot);
    CHECK(whole_run.attached != first.attached);

    const auto note = query::add_note(c, kinds, note_request(a.campaign));
    types::attach_request on_note = screenshot_of_run(a.run);
    on_note.run.reset();
    on_note.note = note;
    const auto noted = query::attach_screenshot(c, on_note);
    CHECK(noted.attachment == first.attachment);
    CHECK(scalar(c, "SELECT count(*) FROM activity.attachment") == "1");
    CHECK(scalar(c, "SELECT count(*) FROM activity.attached") == "3");
}

TEST_CASE("a screenshot is an image on one run or note",
    "[query][campaign]") {
    auto c = fresh();
    const auto a = start(c, "SC-1");
    auto both = screenshot_of_run(a.run);
    both.note = 1;
    CHECK(rejection([&] { query::attach_screenshot(c, both); }) ==
        reason::invalid);
    auto neither = screenshot_of_run(a.run);
    neither.run.reset();
    CHECK(rejection([&] { query::attach_screenshot(c, neither); }) ==
        reason::invalid);
    auto text = screenshot_of_run(a.run);
    text.media_type = "text/html";
    CHECK(rejection([&] { query::attach_screenshot(c, text); }) ==
        reason::invalid);
    auto empty = screenshot_of_run(a.run);
    empty.bytes.clear();
    CHECK(rejection([&] { query::attach_screenshot(c, empty); }) ==
        reason::invalid);
    auto no_step = screenshot_of_run(a.run);
    no_step.step_ord = 7;
    CHECK(rejection([&] { query::attach_screenshot(c, no_step); }) ==
        reason::invalid);
    CHECK(rejection([&] {
        query::attach_screenshot(c, screenshot_of_run(a.run + 100));
    }) == reason::not_found);
    auto no_note = screenshot_of_run(a.run);
    no_note.run.reset();
    no_note.note = 42;
    CHECK(rejection([&] { query::attach_screenshot(c, no_note); }) ==
        reason::not_found);
    CHECK(scalar(c, "SELECT count(*) FROM activity.attachment") == "0");
}

TEST_CASE("a note has a known kind and names its campaign's scenarios and "
    "their steps", "[query][campaign]") {
    auto c = fresh();
    const auto a = start(c, "SC-1");

    auto n = note_request(a.campaign);
    n.scenario_id = "SC-2";
    n.step_ord = 2;
    n.kind = "question";
    const auto id = query::add_note(c, kinds, n);
    CHECK(scalar(c, "SELECT concat_ws(' ', scenario_id, step_ord, kind, "
        "author, state) FROM activity.note WHERE id = " +
        std::to_string(id)) == "SC-2 2 question ana open");

    auto unknown_kind = note_request(a.campaign);
    unknown_kind.kind = "rant";
    CHECK(rejection([&] { query::add_note(c, kinds, unknown_kind); }) ==
        reason::invalid);
    CHECK(rejection([&] {
        query::add_note(c, types::note_kinds{}, note_request(a.campaign));
    }) == reason::invalid);
    auto empty = note_request(a.campaign);
    empty.text = "  ";
    CHECK(rejection([&] { query::add_note(c, kinds, empty); }) ==
        reason::invalid);
    auto other_story = note_request(a.campaign);
    other_story.scenario_id = "SC-3";
    CHECK(rejection([&] { query::add_note(c, kinds, other_story); }) ==
        reason::invalid);
    auto step_alone = note_request(a.campaign);
    step_alone.step_ord = 0;
    CHECK(rejection([&] { query::add_note(c, kinds, step_alone); }) ==
        reason::invalid);
    auto bad_step = note_request(a.campaign);
    bad_step.scenario_id = "SC-1";
    bad_step.step_ord = 3;
    CHECK(rejection([&] { query::add_note(c, kinds, bad_step); }) ==
        reason::invalid);
    CHECK(rejection([&] {
        query::add_note(c, kinds, note_request(a.campaign + 100));
    }) == reason::not_found);
    CHECK(scalar(c, "SELECT count(*) FROM activity.note") == "1");
}

TEST_CASE("addressing a note closes it once and the open counts follow",
    "[query][campaign]") {
    auto c = fresh();
    const auto a = start(c, "SC-1");
    auto on_scenario = note_request(a.campaign);
    on_scenario.scenario_id = "SC-1";
    const auto first = query::add_note(c, kinds, on_scenario);
    query::add_note(c, kinds, on_scenario);
    query::add_note(c, kinds, note_request(a.campaign));

    types::sprint_request sprint;
    sprint.environment = "main";
    const types::view_filter all{"true"};
    auto r = query::what_needs_testing(c, all, sprint);
    CHECK(r.stories[0].open_notes == 3);
    CHECK(r.stories[0].scenarios[0].open_notes == 2);

    types::address_note_request addressed;
    addressed.note = first;
    addressed.reply = "Oiled.";
    addressed.tester = "ben";
    query::address_note(c, addressed);
    CHECK(scalar(c, "SELECT concat_ws(' ', state, reply, addressed_by, "
        "addressed_at IS NOT NULL) FROM activity.note WHERE id = " +
        std::to_string(first)) == "addressed Oiled. ben t");
    r = query::what_needs_testing(c, all, sprint);
    CHECK(r.stories[0].open_notes == 2);
    CHECK(r.stories[0].scenarios[0].open_notes == 1);

    CHECK(rejection([&] { query::address_note(c, addressed); }) ==
        reason::conflict);
    addressed.note = first + 100;
    CHECK(rejection([&] { query::address_note(c, addressed); }) ==
        reason::not_found);
}

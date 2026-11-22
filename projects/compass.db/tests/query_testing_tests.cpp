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
#include "compass.db/query/testing.hpp"

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.db/io/corpus.hpp"
#include "compass.db/io/schema.hpp"
#include "compass.db/ops/extract.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "database.hpp"

namespace {

using namespace compass::db;
using types::scenario_state;

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

std::string sprint_page(std::string_view id, std::string_view title,
    std::string_view state, std::string_view created) {
    return ":PROPERTIES:\n:ID: " + std::string(id) + "\n:END:\n#+title: " +
        std::string(title) + "\n#+type: sprint\n#+created: " +
        std::string(created) + "\n\nThis page documents a sprint.\n\n"
        "* Status\n\n| Field | Value |\n|-------+-------|\n| State | " +
        std::string(state) + " |\n";
}

std::string work_page(std::string_view id, std::string_view type,
    std::string_view title, std::string_view parent, std::string_view state,
    std::string_view audience = "") {
    std::string out = ":PROPERTIES:\n:ID: " + std::string(id) +
        "\n:END:\n#+title: " + std::string(title) + "\n#+type: " +
        std::string(type) + "\n";
    if (!audience.empty())
        out += "#+audience: " + std::string(audience) + "\n";
    return out + "\nThis page documents a " + std::string(type) +
        " in [[id:" + std::string(parent) + "][its parent]].\n\n"
        "* Status\n\n| Field | Value |\n|-------+-------|\n| State | " +
        std::string(state) + " |\n";
}

/**
 * @brief A test_scenario page of @p story whose second step reads
 * @p step_two, with @p result written into its first step and its State
 * row, as older tooling records results in the page.
 */
std::string scenario_page(std::string_view id, std::string_view story,
    std::string_view step_two = "Open the widget.",
    std::string_view result = "", std::string_view state = "PENDING") {
    return ":PROPERTIES:\n:ID: " + std::string(id) + "\n:END:\n"
        "#+title: Test Scenario: " + std::string(id) + "\n"
        "#+type: test_scenario\n\n"
        "This page documents a test scenario in [[id:" + std::string(story) +
        "][its story]].\n\n* Scenario Info\n\n"
        "| Field         | Value |\n|---------------+-------|\n"
        "| Verifies task | [[id:TASK-1][Paint]] |\n"
        "| Parent story  | [[id:" + std::string(story) + "][Story]] |\n"
        "| State         | " + std::string(state) + " |\n\n"
        "* Steps\n\n** Log in\n\n- Sign in.\n\n*** Result\n\n"
        "| Field  | Value |\n|--------+-------|\n| Status | " +
        std::string(result) + " |\n\n** Open\n\n- " + std::string(step_two) +
        "\n";
}

types::environment worktree(std::string name) {
    types::environment e;
    e.name = std::move(name);
    e.root = "/work/" + e.name;
    return e;
}

io::connection fresh() {
    io::connection c(tests::fresh_database());
    io::init(c, "english");
    return c;
}

/**
 * @brief Places @p pages in the environment @p environment, named so that
 * path order is the order given.
 */
void place(io::connection& c, const std::string& environment,
    const std::vector<types::corpus_document>& pages) {
    std::vector<types::placement> placed;
    for (std::size_t i = 0; i < pages.size(); ++i)
        placed.push_back({"p" + std::to_string(10 + i) + "/page.org",
            pages[i].hash});
    io::write_environment(c, worktree(environment), pages, placed);
}

/*
 * Sprint 2 is STARTED and holds STORY-1, with scenarios SC-1 and SC-2, and
 * STORY-2, with none. Sprint 1 is DONE and holds STORY-OLD and SC-OLD.
 */
std::vector<types::corpus_document> sprint_corpus(documents& docs,
    std::string_view sc1_step_two = "Open the widget.",
    std::string_view sc1_result = "") {
    return {
        docs.add(sprint_page("SPRINT-1", "Sprint 1", "DONE", "2026-10-01")),
        docs.add(sprint_page("SPRINT-2", "Sprint 2", "STARTED",
            "2026-11-01")),
        docs.add(work_page("STORY-1", "story", "Build the widget", "SPRINT-2",
            "STARTED")),
        docs.add(work_page("STORY-2", "story", "Ship the widget", "SPRINT-2",
            "BACKLOG")),
        docs.add(work_page("STORY-OLD", "story", "Draw the widget",
            "SPRINT-1", "DONE")),
        docs.add(work_page("TASK-1", "task", "Paint the widget", "STORY-1",
            "DONE")),
        docs.add(work_page("TASK-2", "task", "Oil the widget", "STORY-1",
            "STARTED")),
        docs.add(scenario_page("SC-1", "STORY-1", sc1_step_two, sc1_result)),
        docs.add(scenario_page("SC-2", "STORY-1")),
        docs.add(scenario_page("SC-OLD", "STORY-OLD")),
    };
}

const types::view_filter everything{"true"};

types::sprint_request present(std::string environment = "main") {
    types::sprint_request r;
    r.environment = std::move(environment);
    return r;
}

/**
 * @brief Records a finished run of @p scenario in @p environment with
 * @p outcome, against the definition the environment holds now, as the
 * campaign writes will; outcome empty leaves the run unfinished.
 */
void record_run(io::connection& c, const std::string& environment,
    const std::string& scenario, const std::string& outcome) {
    c.exec(R"sql(
INSERT INTO activity.campaign (environment, story_id)
SELECT $1, s.story_id FROM test.scenario s
JOIN corpus.placement p ON p.hash = s.hash
JOIN corpus.environment e ON e.id = p.environment AND e.name = $1
WHERE s.org_id = $2
ON CONFLICT DO NOTHING
)sql", {environment, scenario});
    c.exec(R"sql(
INSERT INTO activity.run (campaign, scenario_id, definition, scenario_hash,
    tester, finished_at, outcome)
SELECT ca.id, s.org_id, s.definition, s.hash, 'ana',
       CASE WHEN $3 = '' THEN NULL ELSE clock_timestamp() END,
       nullif($3, '')
FROM test.scenario s
JOIN corpus.placement p ON p.hash = s.hash
JOIN corpus.environment e ON e.id = p.environment AND e.name = $1
JOIN activity.campaign ca ON ca.environment = $1
    AND ca.story_id = s.story_id
WHERE s.org_id = $2
)sql", {environment, scenario, outcome});
}

void add_note(io::connection& c, const std::string& environment,
    const std::string& story, const std::optional<std::string>& scenario,
    const std::string& state = "open") {
    c.exec(R"sql(
INSERT INTO activity.campaign (environment, story_id) VALUES ($1, $2)
ON CONFLICT DO NOTHING
)sql", {environment, story});
    c.exec(R"sql(
INSERT INTO activity.note (campaign, scenario_id, kind, text, author, state)
SELECT id, $3, 'bug', 'It squeaks.', 'ana', $4 FROM activity.campaign
WHERE environment = $1 AND story_id = $2
)sql", {environment, story, scenario, state});
}

const types::scenario_status& scenario(const types::testing_response& r,
    std::string_view id) {
    for (const auto& story : r.stories)
        for (const auto& s : story.scenarios)
            if (s.org_id == id)
                return s;
    FAIL("no scenario " << id);
    static const types::scenario_status none;
    return none;
}

scenario_state state(io::connection& c, std::string_view id,
    const std::string& environment = "main") {
    return scenario(query::what_needs_testing(c, everything,
        present(environment)), id).state;
}

}

TEST_CASE("the present sprint's stories carry their scenarios, untested "
    "until a run finishes", "[query][testing]") {
    auto c = fresh();
    documents docs;
    place(c, "main", sprint_corpus(docs));
    const auto r = query::what_needs_testing(c, everything, present());
    CHECK(r.sprint.org_id == "SPRINT-2");
    CHECK(r.sprint.title == "Sprint 2");
    CHECK(r.sprint.state == "STARTED");
    CHECK_FALSE(r.synced_at.empty());
    REQUIRE(r.stories.size() == 2);
    CHECK(r.stories[0].org_id == "STORY-1");
    CHECK(r.stories[0].state == "STARTED");
    CHECK_FALSE(r.stories[0].campaign);
    REQUIRE(r.stories[0].scenarios.size() == 2);
    CHECK(r.stories[0].scenarios[0].org_id == "SC-1");
    CHECK(r.stories[0].scenarios[0].task_id == "TASK-1");
    CHECK(r.stories[0].scenarios[1].org_id == "SC-2");
    for (const auto& s : r.stories[0].scenarios) {
        CHECK(s.state == scenario_state::untested);
        CHECK_FALSE(s.latest_run);
    }
    CHECK(r.stories[1].org_id == "STORY-2");
    CHECK(r.stories[1].scenarios.empty());

    record_run(c, "main", "SC-1", "");
    CHECK(state(c, "SC-1") == scenario_state::untested);
}

TEST_CASE("a scenario's state follows its latest finished run",
    "[query][testing]") {
    auto c = fresh();
    documents docs;
    place(c, "main", sprint_corpus(docs));

    record_run(c, "main", "SC-1", "passed");
    CHECK(state(c, "SC-1") == scenario_state::passed);
    CHECK(state(c, "SC-2") == scenario_state::untested);

    record_run(c, "main", "SC-1", "failed");
    CHECK(state(c, "SC-1") == scenario_state::failed);

    record_run(c, "main", "SC-1", "passed");
    CHECK(state(c, "SC-1") == scenario_state::passed);

    record_run(c, "main", "SC-1", "blocked");
    const auto r = query::what_needs_testing(c, everything, present());
    const auto& s = scenario(r, "SC-1");
    CHECK(s.state == scenario_state::failed);
    REQUIRE(s.latest_run);
    CHECK(s.latest_run->outcome == "blocked");
    CHECK(s.latest_run->tester == "ana");
    CHECK_FALSE(s.latest_run->finished_at.empty());
    CHECK(r.stories[0].campaign);

    record_run(c, "main", "SC-1", "");
    CHECK(state(c, "SC-1") == scenario_state::failed);
}

TEST_CASE("a pass against an earlier definition is changed; results written "
    "into the page do not change it", "[query][testing]") {
    auto c = fresh();
    {
        documents docs;
        place(c, "main", sprint_corpus(docs));
    }
    record_run(c, "main", "SC-1", "passed");
    CHECK(state(c, "SC-1") == scenario_state::passed);

    {
        documents docs;
        place(c, "main", sprint_corpus(docs, "Open the widget.", "PASS"));
    }
    const auto unchanged = query::what_needs_testing(c, everything,
        present());
    CHECK(scenario(unchanged, "SC-1").state == scenario_state::passed);
    CHECK(scenario(unchanged, "SC-1").latest_run->current_definition);

    {
        documents docs;
        place(c, "main", sprint_corpus(docs, "Open the widget twice."));
    }
    const auto changed = query::what_needs_testing(c, everything, present());
    CHECK(scenario(changed, "SC-1").state == scenario_state::changed);
    CHECK_FALSE(scenario(changed, "SC-1").latest_run->current_definition);
    CHECK(scenario(changed, "SC-2").state == scenario_state::untested);

    record_run(c, "main", "SC-1", "failed");
    CHECK(state(c, "SC-1") == scenario_state::failed);
    record_run(c, "main", "SC-1", "passed");
    CHECK(state(c, "SC-1") == scenario_state::passed);
}

TEST_CASE("runs and notes of one environment do not reach another",
    "[query][testing]") {
    auto c = fresh();
    documents docs;
    const auto pages = sprint_corpus(docs);
    place(c, "main", pages);
    place(c, "release", pages);

    record_run(c, "main", "SC-1", "passed");
    record_run(c, "release", "SC-2", "failed");
    add_note(c, "main", "STORY-1", "SC-1");

    const auto main = query::what_needs_testing(c, everything, present());
    CHECK(scenario(main, "SC-1").state == scenario_state::passed);
    CHECK(scenario(main, "SC-2").state == scenario_state::untested);
    CHECK(scenario(main, "SC-1").open_notes == 1);

    const auto release = query::what_needs_testing(c, everything,
        present("release"));
    CHECK(scenario(release, "SC-1").state == scenario_state::untested);
    CHECK(scenario(release, "SC-2").state == scenario_state::failed);
    CHECK(scenario(release, "SC-1").open_notes == 0);
    CHECK(release.stories[0].open_notes == 0);
    CHECK(release.stories[0].campaign != main.stories[0].campaign);
}

TEST_CASE("open notes are counted per scenario and per campaign",
    "[query][testing]") {
    auto c = fresh();
    documents docs;
    place(c, "main", sprint_corpus(docs));
    add_note(c, "main", "STORY-1", "SC-1");
    add_note(c, "main", "STORY-1", "SC-1");
    add_note(c, "main", "STORY-1", "SC-1", "addressed");
    add_note(c, "main", "STORY-1", "SC-2");
    add_note(c, "main", "STORY-1", std::nullopt);
    add_note(c, "main", "STORY-1", std::nullopt, "addressed");

    const auto r = query::what_needs_testing(c, everything, present());
    CHECK(r.stories[0].open_notes == 4);
    CHECK(scenario(r, "SC-1").open_notes == 2);
    CHECK(scenario(r, "SC-2").open_notes == 1);
    CHECK(r.stories[1].open_notes == 0);
}

TEST_CASE("an earlier sprint is asked for by id, title or directory",
    "[query][testing]") {
    auto c = fresh();
    documents docs;
    place(c, "main", sprint_corpus(docs));
    for (const auto* name : {"SPRINT-1", "Sprint 1", "p10"}) {
        auto r = present();
        r.sprint = name;
        const auto out = query::what_needs_testing(c, everything, r);
        CHECK(out.sprint.org_id == "SPRINT-1");
        CHECK(out.sprint.state == "DONE");
        REQUIRE(out.stories.size() == 1);
        CHECK(out.stories[0].org_id == "STORY-OLD");
        REQUIRE(out.stories[0].scenarios.size() == 1);
        CHECK(out.stories[0].scenarios[0].org_id == "SC-OLD");
    }
    auto r = present();
    r.sprint = "Sprint 9";
    CHECK_THROWS_AS(query::what_needs_testing(c, everything, r),
        query::unknown_sprint);
    CHECK_THROWS_AS(query::what_needs_testing(c, everything,
        present("nowhere")), query::unknown_environment);
}

TEST_CASE("the present sprint is the latest created of those STARTED, and "
    "there may be none", "[query][testing]") {
    auto c = fresh();
    documents docs;
    place(c, "main", {
        docs.add(sprint_page("SPRINT-A", "Sprint A", "STARTED",
            "2026-10-01")),
        docs.add(sprint_page("SPRINT-B", "Sprint B", "STARTED",
            "2026-11-01")),
        docs.add(sprint_page("SPRINT-C", "Sprint C", "DONE", "2026-12-01")),
    });
    CHECK(query::what_needs_testing(c, everything, present()).sprint.org_id ==
        "SPRINT-B");

    place(c, "quiet", {
        docs.add(sprint_page("SPRINT-C", "Sprint C", "DONE", "2026-12-01")),
    });
    CHECK_THROWS_AS(query::what_needs_testing(c, everything,
        present("quiet")), query::unknown_sprint);
}

TEST_CASE("the caller's view hides stories and scenarios",
    "[query][testing]") {
    auto c = fresh();
    documents docs;
    place(c, "main", {
        docs.add(sprint_page("SPRINT-2", "Sprint 2", "STARTED",
            "2026-11-01")),
        docs.add(work_page("STORY-1", "story", "Public story", "SPRINT-2",
            "STARTED", "user")),
        docs.add(work_page("STORY-2", "story", "Team story", "SPRINT-2",
            "STARTED")),
        docs.add(scenario_page("SC-1", "STORY-1")),
    });
    const types::view_filter team_only{"d.type <> 'test_scenario'"};
    const auto r = query::what_needs_testing(c, team_only, present());
    REQUIRE(r.stories.size() == 2);
    CHECK(r.stories[0].scenarios.empty());

    const types::view_filter users{"d.audience = 'user' OR d.type = 'sprint'"};
    const auto u = query::what_needs_testing(c, users, present());
    REQUIRE(u.stories.size() == 1);
    CHECK(u.stories[0].org_id == "STORY-1");
}

TEST_CASE("sprint state lists each story's tasks and their states",
    "[query][testing]") {
    auto c = fresh();
    documents docs;
    place(c, "main", sprint_corpus(docs));
    const auto r = query::sprint_state(c, everything, present());
    CHECK(r.sprint.org_id == "SPRINT-2");
    REQUIRE(r.stories.size() == 2);
    CHECK(r.stories[0].story.org_id == "STORY-1");
    CHECK(r.stories[0].story.state == "STARTED");
    CHECK(r.stories[0].story.title == "Build the widget");
    REQUIRE(r.stories[0].tasks.size() == 2);
    CHECK(r.stories[0].tasks[0].org_id == "TASK-1");
    CHECK(r.stories[0].tasks[0].state == "DONE");
    CHECK(r.stories[0].tasks[1].org_id == "TASK-2");
    CHECK(r.stories[0].tasks[1].state == "STARTED");
    CHECK(r.stories[1].story.state == "BACKLOG");
    CHECK(r.stories[1].tasks.empty());

    auto earlier = present();
    earlier.sprint = "Sprint 1";
    const auto old = query::sprint_state(c, everything, earlier);
    REQUIRE(old.stories.size() == 1);
    CHECK(old.stories[0].story.org_id == "STORY-OLD");
    CHECK(old.stories[0].tasks.empty());
}

TEST_CASE("each scenario state has its name", "[query][testing]") {
    CHECK(types::to_string(scenario_state::untested) == "untested");
    CHECK(types::to_string(scenario_state::failed) == "failed");
    CHECK(types::to_string(scenario_state::passed) == "passed");
    CHECK(types::to_string(scenario_state::changed) == "changed");
}

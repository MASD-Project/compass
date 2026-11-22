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
#include "compass.db/io/corpus.hpp"

#include <deque>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.db/io/schema.hpp"
#include "compass.db/ops/extract.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "database.hpp"

namespace {

using namespace compass::db;

/**
 * @brief Documents parsed and extracted as the sync does, kept alive with
 * the sources and tables their rows point into.
 */
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

std::string page(std::string_view title, std::string_view body) {
    return "#+title: " + std::string(title) + "\n#+type: knowledge\n\n* " +
        std::string(title) + "\n\n" + std::string(body) + "\n";
}

std::uint64_t count(io::connection& c, const std::string& table) {
    return std::stoull(std::string(
        c.exec("SELECT count(*) FROM " + table).value(0, 0)));
}

types::environment worktree(std::string name) {
    types::environment e;
    e.name = std::move(name);
    e.root = "/work/" + e.name;
    e.head = "0123456789abcdef0123456789abcdef01234567";
    return e;
}

io::connection fresh() {
    io::connection c(tests::fresh_database());
    io::init(c, "english");
    return c;
}

}

TEST_CASE("an environment's documents are written once and placed",
    "[io][corpus]") {
    auto c = fresh();
    documents docs;
    const std::vector batch{docs.add(page("Alpha", "Ranking by sections.")),
        docs.add(page("Beta", "See [[id:X][Alpha]]."))};
    const std::vector<types::placement> placed{{"a.org", batch[0].hash},
        {"b.org", batch[1].hash}};
    const auto r = io::write_environment(c, worktree("one"), batch, placed);
    CHECK_FALSE(r.retry);
    CHECK(r.inserted == 2);
    CHECK(r.added == 2);
    CHECK(count(c, "corpus.document") == 2);
    CHECK(count(c, "corpus.heading") == 2);
    CHECK(count(c, "corpus.section") == 2);
    CHECK(count(c, "corpus.link") == 1);
    CHECK(count(c, "corpus.placement") == 2);
    CHECK(io::present(c, std::vector{batch[0].hash, batch[1].hash}).size() ==
        2);
    const auto envs = io::environments(c);
    REQUIRE(envs.size() == 1);
    CHECK(envs[0].name == "one");
    CHECK(envs[0].kind == "worktree");
    CHECK(envs[0].documents == 2);
    CHECK_FALSE(envs[0].synced_at.empty());
}

TEST_CASE("sections are searchable with the title, path and text weighted",
    "[io][corpus]") {
    auto c = fresh();
    documents docs;
    const std::vector batch{docs.add(page("Ranking", "Sections are ranked."))};
    io::write_environment(c, worktree("one"), batch,
        std::vector<types::placement>{{"r.org", batch[0].hash}});
    const auto r = c.exec("SELECT tsv::text FROM corpus.section");
    REQUIRE(r.rows() == 1);
    CHECK(std::string(r.value(0, 0)).find("'rank':1A") != std::string::npos);
    CHECK(c.exec("SELECT count(*) FROM corpus.section WHERE tsv @@ "
        "websearch_to_tsquery('english', 'sections ranked')").value(0, 0) ==
        "1");
}

TEST_CASE("a second environment shares the documents of the first",
    "[io][corpus]") {
    auto c = fresh();
    documents docs;
    const auto a = docs.add(page("Alpha", "One."));
    const auto b = docs.add(page("Beta", "Two."));
    const auto b2 = docs.add(page("Beta", "Two, edited."));
    io::write_environment(c, worktree("one"), std::vector{a, b},
        std::vector<types::placement>{{"a.org", a.hash}, {"b.org", b.hash}});

    const std::vector wanted{a.hash, b2.hash};
    const auto have = io::present(c, wanted);
    REQUIRE(have.size() == 1);
    CHECK(have[0] == a.hash);
    const auto r = io::write_environment(c, worktree("two"), std::vector{b2},
        std::vector<types::placement>{{"a.org", a.hash}, {"b.org", b2.hash}});
    CHECK(r.inserted == 1);
    CHECK(count(c, "corpus.document") == 3);
    CHECK(count(c, "corpus.placement") == 4);
}

TEST_CASE("a sync replaces only what changed", "[io][corpus]") {
    auto c = fresh();
    documents docs;
    const auto a = docs.add(page("Alpha", "One."));
    const auto b = docs.add(page("Beta", "Two."));
    const auto b2 = docs.add(page("Beta", "Three."));
    io::write_environment(c, worktree("one"), std::vector{a, b},
        std::vector<types::placement>{{"a.org", a.hash}, {"b.org", b.hash}});

    const auto unchanged = io::write_environment(c, worktree("one"), {},
        std::vector<types::placement>{{"a.org", a.hash}, {"b.org", b.hash}});
    CHECK(unchanged.inserted + unchanged.added + unchanged.changed +
            unchanged.removed == 0);

    const auto edited = io::write_environment(c, worktree("one"),
        std::vector{b2}, std::vector<types::placement>{{"b.org", b2.hash}});
    CHECK(edited.changed == 1);
    CHECK(edited.removed == 1);
    CHECK(count(c, "corpus.placement") == 1);
}

TEST_CASE("a scoped sync changes only the paths it names", "[io][corpus]") {
    auto c = fresh();
    documents docs;
    const auto a = docs.add(page("Alpha", "One."));
    const auto b = docs.add(page("Beta", "Two."));
    const auto a2 = docs.add(page("Alpha", "One, edited."));
    io::write_environment(c, worktree("one"), std::vector{a, b},
        std::vector<types::placement>{{"a.org", a.hash}, {"b.org", b.hash}});

    const std::vector<std::string> one{"a.org"};
    const auto r = io::write_environment(c, worktree("one"), std::vector{a2},
        std::vector<types::placement>{{"a.org", a2.hash}}, one);
    CHECK(r.changed == 1);
    CHECK(r.removed == 0);
    CHECK(count(c, "corpus.placement") == 2);

    const std::vector<std::string> gone{"b.org"};
    const auto removed = io::write_environment(c, worktree("one"), {}, {},
        gone);
    CHECK(removed.removed == 1);
    CHECK(count(c, "corpus.placement") == 1);
}

TEST_CASE("a placement of a document the corpus lacks asks for a retry",
    "[io][corpus]") {
    auto c = fresh();
    documents docs;
    const auto a = docs.add(page("Alpha", "One."));
    const auto r = io::write_environment(c, worktree("one"), {},
        std::vector<types::placement>{{"a.org", a.hash}});
    CHECK(r.retry);
    REQUIRE(r.missing.size() == 1);
    CHECK(r.missing[0] == a.hash);
    CHECK(count(c, "corpus.environment") == 0);
}

TEST_CASE("a test scenario fills the test schema", "[io][corpus]") {
    auto c = fresh();
    documents docs;
    const auto s = docs.add(R"(#+title: Test Scenario: Log in
#+type: test_scenario

* Scenario Info

| Field         | Value |
|---------------+-------|
| Verifies task | [[id:TASK][The task]] |
| Parent story  | [[id:STORY][The story]] |

* Steps

** Sign in

- Sign in.

** Sign out

- Sign out.
)");
    io::write_environment(c, worktree("one"), std::vector{s},
        std::vector<types::placement>{{"s.org", s.hash}});
    const auto r = c.exec("SELECT task_id, story_id FROM test.scenario");
    REQUIRE(r.rows() == 1);
    CHECK(r.value(0, 0) == "TASK");
    CHECK(r.value(0, 1) == "STORY");
    CHECK(count(c, "test.step") == 2);
}

TEST_CASE("collection removes what no environment and no run names",
    "[io][corpus]") {
    auto c = fresh();
    documents docs;
    const auto shared = docs.add(page("Shared", "In both."));
    const auto only = docs.add(page("Only", "In the dropped one."));
    const auto tested = docs.add(page("Tested", "A run names it."));
    io::write_environment(c, worktree("kept"), std::vector{shared},
        std::vector<types::placement>{{"s.org", shared.hash}});
    io::write_environment(c, worktree("dropped"), std::vector{only, tested},
        std::vector<types::placement>{{"s.org", shared.hash},
            {"o.org", only.hash}, {"t.org", tested.hash}});
    c.exec("INSERT INTO activity.campaign (environment, story_id) VALUES "
        "('dropped', 'STORY')");
    c.exec("INSERT INTO activity.run (campaign, scenario_id, definition, "
        "scenario_hash, tester) SELECT id, 'S', '\\x00', $1::bytea, 'tester' "
        "FROM activity.campaign",
        {"\\x" + compass::store::ops::to_hex(tested.hash)});

    CHECK(io::drop_environment(c, "dropped"));
    CHECK_FALSE(io::drop_environment(c, "dropped"));
    const auto r = io::collect(c);
    CHECK(r.ran);
    CHECK(r.removed == 1);
    CHECK(r.left == 2);
    CHECK(io::present(c, std::vector{only.hash}).empty());
    CHECK(io::present(c, std::vector{shared.hash, tested.hash}).size() == 2);
    CHECK(count(c, "corpus.section") == 2);

    CHECK(io::collect(c).removed == 0);
}

TEST_CASE("collection as part of a sync waits for the corpus to double",
    "[io][corpus]") {
    auto c = fresh();
    CHECK_FALSE(io::collect_when_grown(c).ran);
}

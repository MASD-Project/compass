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
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.cli/app/application.hpp"
#include "compass.db/io/connection.hpp"
#include "compass.db/query/campaign.hpp"
#include "compass.testing/database/scoped_cluster.hpp"
#include "scoped_git_repo.hpp"

namespace {

namespace fs = std::filesystem;
using namespace compass::cli;
using tests::scoped_git_repo;
using Catch::Matchers::ContainsSubstring;

struct outcome {
    int status = -1;
    std::string out;
    std::string err;
};

outcome run(std::vector<std::string> args) {
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

std::string fresh_database() {
    std::string why;
    auto* cluster = compass::testing::database::shared_cluster(
        "compass.cli.tests", why);
    if (cluster == nullptr)
        SKIP(why);
    return cluster->create_database();
}

const std::string story_folder = "doc/agile/versions/v0/sprint_01/s1";
const std::string page_path = story_folder + "/test_campaign_1.org";
const std::string scenario_path = story_folder + "/scenario_a.org";

/**
 * @brief A repository with a story and one scenario of two steps, synced
 * into a fresh database as the environment main, with a finished run of
 * the scenario that failed its second step and attached a screenshot.
 */
struct campaign_repo {
    scoped_git_repo repo;
    std::string database = fresh_database();
    compass::db::io::connection c{database};
    std::int64_t campaign = 0;

    campaign_repo() {
        repo.write(story_folder + "/story.org", ":PROPERTIES:\n:ID: STORY-1\n"
            ":END:\n#+title: Story: One\n#+type: story\n");
        repo.write(scenario_path, ":PROPERTIES:\n:ID: SC-1\n:END:\n"
            "#+title: Test Scenario: A\n#+type: test_scenario\n\n"
            "* Scenario Info\n\n| Field | Value |\n|-------+-------|\n"
            "| Parent story | [[id:STORY-1][Story: One]] |\n\n"
            "* Steps\n\n** Log in\n\n- Sign in.\n\n** Open\n\n- Open it.\n");
        REQUIRE(command({"db", "init", "--database", database}).status == 0);
        REQUIRE(command({"sync", "--environment", "main"}).status == 0);

        compass::db::types::start_run_request start;
        start.environment = "main";
        start.scenario_id = "SC-1";
        start.tester = "ana";
        const auto run = compass::db::query::start_run(c, start);
        campaign = run.campaign;
        using compass::db::types::step_outcome;
        for (const auto result : {step_outcome::pass, step_outcome::fail}) {
            compass::db::types::step_result_request r;
            r.run = run.run;
            r.step_ord = result == step_outcome::pass ? 0 : 1;
            r.outcome = result;
            r.tester = "ana";
            compass::db::query::record_step_result(c, r);
        }
        compass::db::types::attach_request a;
        a.run = run.run;
        a.step_ord = 1;
        a.media_type = "image/png";
        a.bytes = "\x89PNG\r\n\x1a\n";
        a.tester = "ana";
        compass::db::query::attach_screenshot(c, a);
        compass::db::types::finish_run_request f;
        f.run = run.run;
        f.tester = "ana";
        compass::db::query::finish_run(c, f);
    }

    outcome command(std::vector<std::string> args) const {
        args.insert(args.begin() + 1, {"-C", repo.checkout().generic_string(),
            "-j", "2"});
        return run(std::move(args));
    }

    outcome export_main() const {
        return command({"test", "export", "--environment", "main"});
    }

    [[nodiscard]] fs::path at(const std::string& relative) const {
        return repo.checkout() / relative;
    }
};

/**
 * @brief The :ID: line after @p heading in @p page.
 */
std::string id_after(const std::string& page, std::string_view heading) {
    const auto at = page.find(":ID: ", page.find(heading));
    return page.substr(at, page.find('\n', at) - at);
}

}

TEST_CASE("test export writes the campaign page and its screenshot into the "
    "story's folder and leaves the scenario page alone", "[app][test_export]") {
    campaign_repo s;
    REQUIRE(s.campaign == 1);
    const auto scenario = scoped_git_repo::read_file(s.at(scenario_path));
    const auto r = s.export_main();
    CHECK(r.status == app::exit_success);
    CHECK_THAT(r.out, ContainsSubstring("exported 1 test campaigns of main: "
        "2 files written, 0 unchanged"));
    const auto page = scoped_git_repo::read_file(s.at(page_path));
    CHECK_THAT(page, ContainsSubstring("#+type: test_campaign\n"));
    CHECK_THAT(page, ContainsSubstring("*** Run 1: failed\n"));
    CHECK(fs::is_directory(s.at(story_folder + "/test_campaign_1")));
    CHECK(scoped_git_repo::read_file(s.at(scenario_path)) == scenario);
    compass::db::io::connection c(s.database);
    CHECK(c.exec("SELECT exported_at IS NOT NULL FROM activity.campaign")
              .value(0, 0) == "t");
}

TEST_CASE("test export of an unchanged campaign writes nothing",
    "[app][test_export]") {
    campaign_repo s;
    REQUIRE(s.export_main().status == app::exit_success);
    const auto page = s.at(page_path);
    const auto before = fs::last_write_time(page);
    const auto bytes = scoped_git_repo::read_file(page);
    const auto again = s.export_main();
    CHECK(again.status == app::exit_success);
    CHECK_THAT(again.out, ContainsSubstring("0 files written, 2 unchanged"));
    CHECK_THAT(again.out, !ContainsSubstring("compass: wrote"));
    // Compared outside CHECK: Catch2 cannot print libc++'s file clock, whose
    // representation is __int128.
    const bool untouched = fs::last_write_time(page) == before;
    CHECK(untouched);
    CHECK(scoped_git_repo::read_file(page) == bytes);
}

TEST_CASE("test export keeps the page's path and :ID: when the campaign "
    "changes, and rewrites only the page", "[app][test_export]") {
    campaign_repo s;
    REQUIRE(s.export_main().status == app::exit_success);
    const auto first = scoped_git_repo::read_file(s.at(page_path));

    compass::db::types::add_note_request n;
    n.campaign = s.campaign;
    n.scenario_id = "SC-1";
    n.step_ord = 1;
    n.kind = "defect";
    n.text = "The list is empty.";
    n.tester = "ben";
    const auto note = compass::db::query::add_note(s.c, {{"defect"}}, n);

    const auto r = s.export_main();
    CHECK(r.out == "compass: wrote " + page_path + "\ncompass: exported 1 "
        "test campaigns of main: 1 files written, 1 unchanged\n");
    const auto second = scoped_git_repo::read_file(s.at(page_path));
    CHECK(id_after(second, "") == id_after(first, ""));
    CHECK(id_after(second, "*** Run 1") == id_after(first, "*** Run 1"));
    CHECK_THAT(second, ContainsSubstring("*** TODO Note " +
        std::to_string(note) + " on [[id:SC-1][Test Scenario: A]], step 2\n"));
}

TEST_CASE("test export of an environment with no campaign writes nothing, "
    "and of one that does not exist fails", "[app][test_export]") {
    campaign_repo s;
    const auto none = s.command({"test", "export", "--environment", "main",
        "--story", "STORY-2"});
    CHECK(none.status == app::exit_success);
    CHECK_THAT(none.out, ContainsSubstring("exported 0 test campaigns"));
    CHECK_FALSE(fs::exists(s.at(page_path)));
    const auto missing = s.command({"test", "export", "--environment",
        "nowhere"});
    CHECK(missing.status == app::exit_failure);
    CHECK_THAT(missing.err, ContainsSubstring("no environment named nowhere"));
}

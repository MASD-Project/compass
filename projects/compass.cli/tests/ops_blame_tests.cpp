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
#include "compass.cli/ops/blame.hpp"

#include <sstream>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.cli/io/report.hpp"

namespace {

using namespace compass::cli;
using Catch::Matchers::ContainsSubstring;
using compass::codec::types::blame_method;

types::blamed_finding traced(std::string path, std::string environment,
    bool on_main = true, blame_method method = blame_method::line) {
    types::blamed_finding b;
    b.found.path = std::move(path);
    b.found.line = 3;
    b.found.rule = "sql_hygiene/not_re_runnable";
    b.found.message = "create table without if not exists";
    b.found.fix = "add if not exists";
    b.method = method;
    types::culprit c;
    c.id = "8bf2f80aa1b2c3d4e5f60718293a4b5c6d7e8f90";
    c.subject = "[sql] Add the table";
    c.author = "Marco";
    c.pull_request = 4966;
    c.trailers = {{"Story-ID", "", ""}, {"Environment", environment, ""}};
    c.environment = std::move(environment);
    c.on_main = on_main;
    b.culprits.push_back(std::move(c));
    return b;
}

}

TEST_CASE("blame reads the commit of each line and each commit's time from "
    "git blame --porcelain", "[ops][blame]") {
    const std::string a(40, 'a');
    const std::string b(40, 'b');
    const std::vector<std::string> lines{a + " 1 3 1", "author x",
        "committer-time 100", "\tline three", b + " 7 9 1",
        "committer-time 200", "\tline nine", a + " 2 4", "\tline four"};
    const auto r = ops::parse_blame_porcelain(lines);
    CHECK(r.commits.at(3) == a);
    CHECK(r.commits.at(4) == a);
    CHECK(r.commits.at(9) == b);
    CHECK(r.times.at(a) == 100);
    CHECK(r.times.at(b) == 200);
}

TEST_CASE("blame finds the pull request a merge or a squash subject names",
    "[ops][blame]") {
    CHECK(ops::pull_request_in("Merge pull request #4966 from o/feature/x") ==
        4966u);
    CHECK(ops::pull_request_in("[sql] Add the table (#12)") == 12u);
    CHECK_FALSE(ops::pull_request_in("[sql] Add the table"));
    CHECK_FALSE(ops::pull_request_in("Merge branch 'main'"));
}

TEST_CASE("blame finds the lines of a generated file that differ from its "
    "regeneration", "[ops][blame]") {
    CHECK(ops::differing_lines("a\nb\nX\nc\n", "a\nb\nc\n") ==
        std::pair<std::uint32_t, std::uint32_t>{3, 3});
    CHECK(ops::differing_lines("a\nX\nY\nd\n", "a\nb\nd\n") ==
        std::pair<std::uint32_t, std::uint32_t>{2, 3});
    CHECK(ops::differing_lines("a\nc\n", "a\nb\nc\n") ==
        std::pair<std::uint32_t, std::uint32_t>{1, 1});
}

TEST_CASE("blame reads a finding as check prints it", "[ops][blame]") {
    const auto t = ops::parse_target("src/a:b.cpp:12:portable_cpp/atomic");
    REQUIRE(t);
    CHECK(t->path == "src/a:b.cpp");
    CHECK(t->line == 12);
    CHECK(t->rule == "portable_cpp/atomic");
    CHECK_FALSE(ops::parse_target("src/a.cpp:12"));
    CHECK_FALSE(ops::parse_target("src/a.cpp:x:codec/rule"));
    CHECK_FALSE(ops::parse_target("src/a.cpp:12:rule"));
}

TEST_CASE("blame sends one message per environment, those that state none "
    "last, and leaves out the branch's own", "[ops][blame]") {
    types::blame_report r;
    r.notify = "Environment";
    r.findings = {traced("b.sql", "jolly_knuth"), traced("a.sql", ""),
        traced("c.sql", "brave_hopper"), traced("d.sql", "jolly_knuth"),
        traced("e.sql", "jolly_knuth", false)};
    const auto notices = ops::notices_of(r);
    REQUIRE(notices.size() == 3);
    CHECK(notices[0].environment == "brave_hopper");
    CHECK(notices[1].environment == "jolly_knuth");
    REQUIRE(notices[1].findings.size() == 2);
    CHECK(notices[1].findings[0]->found.path == "b.sql");
    CHECK(notices[1].findings[1]->found.path == "d.sql");
    CHECK(notices[2].environment.empty());
    CHECK(notices[2].findings.front()->found.path == "a.sql");

    std::ostringstream out;
    io::write_blame(out, r);
    const auto text = out.str();
    CHECK_THAT(text, ContainsSubstring("compass blame: 4 findings already on "
        "main, traced to 2 environments; 1 from commits main does not hold "
        "yet; 0 not traced"));
    CHECK_THAT(text, ContainsSubstring("These findings were already on main. "
        "Tell environment jolly_knuth that it broke them, with these "
        "details:\n- sql_hygiene/not_re_runnable at b.sql:3: create table "
        "without if not exists\n  - commit 8bf2f80aa1 '[sql] Add the table' "
        "in PR #4966, by Marco, Story-ID not stated, Environment "
        "jolly_knuth\n  - found by: line blame"));
    CHECK_THAT(text, ContainsSubstring("the commits that broke them state no "
        "Environment trailer. Tell the author of each commit, through its "
        "pull request"));
    CHECK_THAT(text, ContainsSubstring("  - fix: add if not exists\n"));
    CHECK_THAT(text, !ContainsSubstring("e.sql"));
}

TEST_CASE("blame reads trailers set apart from a closing Co-Authored-By",
    "[ops][blame]") {
    const auto t = ops::closing_trailers("[inbox] Address the review\n\n"
        "Give the grants a section.\n\nStory-ID: S\nTask-ID: T\n"
        "Environment: bright_faraday\n\nCo-Authored-By: C <c@example.org>\n");
    REQUIRE(t.size() == 4);
    CHECK(t[0].key == "Co-Authored-By");
    CHECK(t[3].key == "Environment");
    CHECK(t[3].value == "bright_faraday");
    CHECK(ops::closing_trailers("Subject only\n").empty());
    CHECK(ops::closing_trailers("Subject\n\nA body of prose.\n\n"
        "Environment: x_y\n").size() == 1);
}

TEST_CASE("blame says likely for a guess", "[ops][blame]") {
    types::blame_report r;
    r.notify = "Environment";
    auto b = traced("a.sql", "jolly_knuth", true, blame_method::added);
    b.subject = "a.sql";
    r.findings = {b};
    std::ostringstream out;
    io::write_blame(out, r);
    CHECK_THAT(out.str(), ContainsSubstring("that it likely broke them"));
    CHECK_THAT(out.str(), ContainsSubstring("found by: added blame, likely: "
        "the commit that added a.sql"));
}

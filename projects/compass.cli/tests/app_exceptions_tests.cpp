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
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.cli/app/application.hpp"
#include "scoped_git_repo.hpp"

namespace {

using namespace compass::cli;
using tests::scoped_git_repo;
using Catch::Matchers::ContainsSubstring;

constexpr std::string_view configuration =
    "#+output: out\n\n* Codecs\n\n"
    "| name | enabled | options |\n"
    "| document_index | no | |\n"
    "| required_sections | no | |\n"
    "| sql_hygiene | yes | roots=sql |\n";

constexpr std::string_view violation = "create table a (x int);";
constexpr std::string_view clean = "select 1;";

std::string exception(std::string_view expires = "2026-12-31") {
    return " -- compass:except(sql_hygiene/not_re_runnable) expires=" +
        std::string(expires) + " approver=marco reason=bootstrap table\n";
}

void commit_all(const scoped_git_repo& repo) {
    repo.git("add -A");
    repo.git("-c user.name=t -c user.email=t@example.org "
             "-c commit.gpgsign=false commit -q -m c");
}

struct outcome {
    int status = -1;
    std::string out;
    std::string err;

    /**
     * @brief The findings of out, as path:line:rule.
     */
    [[nodiscard]] std::vector<std::string> findings() const {
        static const std::regex finding(
            "^([^: ]+:[0-9]+):[0-9]+: ([^: ]+): .*");
        std::vector<std::string> lines;
        std::istringstream in(out);
        std::smatch m;
        for (std::string line; std::getline(in, line);) {
            if (std::regex_match(line, m, finding))
                lines.push_back(m[1].str() + ":" + m[2].str());
        }
        return lines;
    }
};

/**
 * @brief @p config with an exceptions table of @p rows, whose first row
 * is on line 13 for the configurations above.
 */
std::string with_table(std::string_view config, std::string_view rows) {
    return std::string(config) + "\n* Exceptions\n\n"
        "| path | line | rule | expires | approver | reason |\n" +
        std::string(rows);
}

std::string row(std::string_view path, std::string_view line,
    std::string_view rule, std::string_view expires = "2026-12-31") {
    return "| " + std::string(path) + " | " + std::string(line) + " | " +
        std::string(rule) + " | " + std::string(expires) +
        " | marco | generated from a shared template |\n";
}

outcome check(const scoped_git_repo& repo, std::string_view today,
    std::vector<std::string> extra = {}) {
    std::vector<std::string> args{"check", "-C",
        repo.checkout().generic_string(), "-j", "2", "--today",
        std::string(today)};
    args.insert(args.end(), extra.begin(), extra.end());
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

}

TEST_CASE("a valid exception excuses the finding it covers",
    "[app][exceptions]") {
    scoped_git_repo repo;
    repo.write(".compass.org", configuration);
    repo.write("sql/a.sql", std::string(violation) + exception());

    const auto r = check(repo, "2026-11-16");
    CHECK(r.status == 0);
    CHECK(r.findings().empty());
    CHECK_THAT(r.out, ContainsSubstring("0 diagnostics, 1 excused by "
                                        "exception"));
}

TEST_CASE("an expired exception reports the finding and its expiry on the "
          "line it covers", "[app][exceptions]") {
    scoped_git_repo repo;
    repo.write(".compass.org", configuration);
    repo.write("sql/a.sql", std::string(clean) + "\n" +
        "-- compass:except(sql_hygiene/not_re_runnable) expires=2026-11-15 "
        "approver=marco reason=bootstrap table\n" + std::string(violation) +
        "\n");

    const auto r = check(repo, "2026-11-16");
    CHECK(r.status == 1);
    CHECK(r.findings() == std::vector<std::string>{
        "sql/a.sql:3:sql_hygiene/not_re_runnable",
        "sql/a.sql:3:exception/expired"});
}

TEST_CASE("a result reused from the record does not hide an exception "
          "that has since expired", "[app][exceptions]") {
    scoped_git_repo repo;
    repo.write(".compass.org", configuration);
    repo.write("sql/a.sql", std::string(violation) + exception("2026-11-30"));

    CHECK(check(repo, "2026-11-30").status == 0);
    const auto later = check(repo, "2026-12-01");
    CHECK(later.status == 1);
    CHECK(later.findings() == std::vector<std::string>{
        "sql/a.sql:1:sql_hygiene/not_re_runnable",
        "sql/a.sql:1:exception/expired"});
    CHECK_THAT(later.out, ContainsSubstring(" sql_hygiene 0"));
}

TEST_CASE("an exception added to a file checked before is seen",
    "[app][exceptions]") {
    scoped_git_repo repo;
    repo.write(".compass.org", configuration);
    repo.write("sql/a.sql", std::string(violation) + "\n");
    CHECK(check(repo, "2026-11-16").status == 1);

    repo.write("sql/a.sql", std::string(violation) + exception());
    CHECK(check(repo, "2026-11-16").status == 0);
    repo.write("sql/a.sql", std::string(violation) + "\n");
    CHECK(check(repo, "2026-11-16").findings() == std::vector<std::string>{
        "sql/a.sql:1:sql_hygiene/not_re_runnable"});
}

TEST_CASE("an exception that covers no finding is stale",
    "[app][exceptions]") {
    scoped_git_repo repo;
    repo.write(".compass.org", configuration);
    repo.write("sql/a.sql", std::string(clean) + exception());

    const auto r = check(repo, "2026-11-16");
    CHECK(r.status == 1);
    CHECK(r.findings() == std::vector<std::string>{
        "sql/a.sql:1:exception/stale"});
}

TEST_CASE("check of a commit sees the exceptions of that commit",
    "[app][exceptions]") {
    scoped_git_repo repo;
    repo.write(".compass.org", configuration);
    repo.write("sql/a.sql", std::string(violation) + exception());
    commit_all(repo);
    repo.write("sql/a.sql", std::string(violation) + "\n");
    commit_all(repo);

    CHECK(check(repo, "2026-11-16", {"--commit", "HEAD~1"}).status == 0);
    CHECK(check(repo, "2026-11-16", {"--commit", "HEAD"}).findings() ==
        std::vector<std::string>{"sql/a.sql:1:sql_hygiene/not_re_runnable"});
}

TEST_CASE("ratchet sees the exceptions of each tree", "[app][exceptions]") {
    scoped_git_repo repo;
    repo.write(".compass.org", configuration);
    repo.write("sql/a.sql", std::string(violation) + "\n");
    commit_all(repo);
    repo.git("branch base");

    SECTION("an exception added for an old finding is not new") {
        repo.write("sql/a.sql", std::string(violation) + exception());
        commit_all(repo);
        const auto r = check(repo, "2026-11-16", {"--ratchet", "base"});
        CHECK(r.status == 0);
        CHECK_THAT(r.out, ContainsSubstring("0 new diagnostics, 1 excused "
                                            "by exception"));
    }
    SECTION("an exception left behind by a fix is new") {
        repo.write("sql/a.sql", std::string(clean) + exception());
        commit_all(repo);
        const auto r = check(repo, "2026-11-16", {"--ratchet", "base"});
        CHECK(r.status == 1);
        CHECK(r.findings() == std::vector<std::string>{
            "sql/a.sql:1:exception/stale"});
    }
}

TEST_CASE("a row of the exceptions table excuses a finding on an org "
          "document by line or for the whole file", "[app][exceptions]") {
    scoped_git_repo repo;
    const auto documents = std::regex_replace(std::string(configuration),
        std::regex("required_sections \\| no"), "required_sections | yes");
    constexpr std::string_view rule = "required_sections/missing_section";

    repo.write(".compass.org", with_table(documents,
        row("doc/task_b.org", "2", rule)));
    const auto by_line = check(repo, "2026-11-16");
    CHECK(by_line.status == 0);
    CHECK_THAT(by_line.out, ContainsSubstring("0 diagnostics, 1 excused by "
                                              "exception"));

    repo.write(".compass.org", with_table(documents,
        row("doc/task_b.org", "", rule)));
    CHECK(check(repo, "2026-11-16").status == 0);

    repo.write(".compass.org", with_table(documents,
        row("doc/task_b.org", "3", rule)));
    CHECK(check(repo, "2026-11-16").findings() == std::vector<std::string>{
        ".compass.org:13:exception/stale",
        "doc/task_b.org:2:required_sections/missing_section"});
}

TEST_CASE("a row that does not hold is reported on its line of "
          ".compass.org and excuses nothing", "[app][exceptions]") {
    scoped_git_repo repo;
    repo.write(".compass.org", with_table(configuration,
        row("sql/a.sql", "1", "sql_hygiene/not_re_runnable", "2026-11-15") +
        row("sql/a.sql", "1", "sql_hygiene/not_re_runnable", "") +
        "| sql/a.sql | 1 | sql_hygiene/not_re_runnable |\n" +
        row("sql/a.sql", "1", "sql_hygiene/no_such_rule")));
    repo.write("sql/a.sql", std::string(violation) + "\n");

    const auto r = check(repo, "2026-11-16");
    CHECK(r.status == 1);
    CHECK(r.findings() == std::vector<std::string>{
        ".compass.org:13:exception/expired",
        ".compass.org:14:exception/incomplete",
        ".compass.org:15:exception/malformed",
        ".compass.org:16:exception/malformed",
        "sql/a.sql:1:sql_hygiene/not_re_runnable"});
    CHECK(check(repo, "2026-11-15").findings() == std::vector<std::string>{
        ".compass.org:14:exception/incomplete",
        ".compass.org:15:exception/malformed",
        ".compass.org:16:exception/malformed"});
}

TEST_CASE("check of a commit applies the exceptions table of that commit",
    "[app][exceptions]") {
    scoped_git_repo repo;
    repo.write(".compass.org", with_table(configuration,
        row("sql/a.sql", "1", "sql_hygiene/not_re_runnable")));
    repo.write("sql/a.sql", std::string(violation) + "\n");
    commit_all(repo);
    repo.write(".compass.org", configuration);
    commit_all(repo);
    repo.write(".compass.org", with_table(configuration,
        row("sql/a.sql", "1", "sql_hygiene/not_re_runnable")));

    CHECK(check(repo, "2026-11-16", {"--commit", "HEAD~1"}).status == 0);
    CHECK(check(repo, "2026-11-16", {"--commit", "HEAD"}).findings() ==
        std::vector<std::string>{"sql/a.sql:1:sql_hygiene/not_re_runnable"});
}

TEST_CASE("ratchet applies the checkout's exceptions table to both sides",
    "[app][exceptions]") {
    scoped_git_repo repo;
    repo.write(".compass.org", configuration);
    repo.write("sql/a.sql", std::string(violation) + "\n");
    commit_all(repo);
    repo.git("branch base");

    SECTION("a row added for an old finding is not new") {
        repo.write(".compass.org", with_table(configuration,
            row("sql/a.sql", "1", "sql_hygiene/not_re_runnable")));
        commit_all(repo);
        const auto r = check(repo, "2026-11-16", {"--ratchet", "base"});
        CHECK(r.status == 0);
        CHECK_THAT(r.out, ContainsSubstring("0 new diagnostics, 1 excused "
                                            "by exception"));
    }
    SECTION("a row left behind by a fix is new") {
        repo.write(".compass.org", with_table(configuration,
            row("sql/a.sql", "1", "sql_hygiene/not_re_runnable")));
        repo.write("sql/a.sql", std::string(clean) + "\n");
        commit_all(repo);
        const auto r = check(repo, "2026-11-16", {"--ratchet", "base"});
        CHECK(r.status == 1);
        CHECK(r.findings() == std::vector<std::string>{
            ".compass.org:13:exception/stale"});
    }
    SECTION("a row whose line no longer holds the finding is new") {
        repo.write(".compass.org", with_table(configuration,
            row("sql/a.sql", "1", "sql_hygiene/not_re_runnable")));
        repo.write("sql/a.sql", std::string(clean) + "\n" +
            std::string(violation) + "\n");
        commit_all(repo);
        const auto r = check(repo, "2026-11-16", {"--ratchet", "base"});
        CHECK(r.status == 1);
        CHECK(r.findings() == std::vector<std::string>{
            ".compass.org:13:exception/stale",
            "sql/a.sql:2:sql_hygiene/not_re_runnable"});
    }
}

TEST_CASE("The --today option takes a date and applies to check only",
    "[app][exceptions]") {
    scoped_git_repo repo;
    repo.write(".compass.org", configuration);
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto checkout = repo.checkout().generic_string();

    const std::vector<std::string_view> bad{"check", "-C", checkout,
        "--today", "2026-02-30"};
    CHECK(app::run(bad, "/", in, out, err) == 2);
    CHECK_THAT(err.str(), ContainsSubstring("bad value for --today"));

    const std::vector<std::string_view> generate{"generate", "-C", checkout,
        "--today", "2026-11-16"};
    CHECK(app::run(generate, "/", in, out, err) == 2);
    CHECK_THAT(err.str(), ContainsSubstring("--today applies to check, hook, "
        "blame and env version new only"));
}

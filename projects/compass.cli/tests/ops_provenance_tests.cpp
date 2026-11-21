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
#include "compass.cli/ops/provenance.hpp"

#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.cli/io/history.hpp"
#include "compass.codec/ops/data_tables.hpp"

namespace {

using namespace compass::cli;
using Catch::Matchers::ContainsSubstring;

constexpr std::string_view story = "E6359881-7919-4BBC-9B9C-03A8EEDB8A03";
constexpr std::string_view task = "C6436C51-A6DB-422B-AC5E-B60255F90219";
constexpr std::string_view other_task =
    "67AF2BC8-0FD7-4382-A4B3-C2407C408708";

/**
 * @brief The rules ORE Studio states: story and task by :ID:, in the third
 * cell of their rows, and the environment by name, in the second.
 */
types::provenance_rules ore_rules() {
    return {.source = "compass/checks/commit_provenance.org",
        .trailers = {
            {"Story-ID", "story", "", "Story", 3, 10},
            {"Task-ID", "task", "", "Task", 3, 11},
            {"Environment", "", "[a-z]+_[a-z]+", "Environment", 2, 12}},
        .exemptions = {{"^\\[agile\\] Record PR #", {"Environment"}}},
        .require_pull_request = false};
}

types::document_ids documents() {
    return {{std::string(story), {"doc/story.org", "story"}},
        {std::string(task), {"doc/task.org", "task"}},
        {std::string(other_task), {"doc/other/task.org", "task"}}};
}

types::branch_commit commit(std::string subject,
    std::vector<types::trailer> trailers, std::size_t parents = 1) {
    return {.id = "0123456789abcdef0123456789abcdef01234567",
        .parents = parents, .subject = std::move(subject),
        .trailers = std::move(trailers)};
}

std::vector<types::trailer> complete(std::string_view t = task) {
    return {{"Story-ID", std::string(story), 3},
        {"Task-ID", std::string(t), 4}, {"Environment", "jolly_knuth", 5}};
}

std::vector<std::string> rules_of(const types::provenance_result& r) {
    std::vector<std::string> out;
    for (const auto& f : r.findings)
        out.push_back(f.path + ":" + std::to_string(f.line) + ":" + f.rule);
    return out;
}

const std::string ore_body =
    "## Summary\n\nText.\n\n## Traceability\n\n"
    "| Artefact | Link | ID |\n"
    "|----------|------|----|\n"
    "| Story | [Design the scripted trade subsystem](https://x/story.html) | "
    "E6359881-7919-4BBC-9B9C-03A8EEDB8A03 |\n"
    "| Task | [Copy ORE's script parser](https://x/task.html) | "
    "C6436C51-A6DB-422B-AC5E-B60255F90219 |\n"
    "| Environment | jolly_knuth | |\n";

}

TEST_CASE("the trailers of a message are the lines of its last paragraph",
    "[ops][provenance]") {
    const auto t = ops::trailers_of("[cli] Do a thing\n\nBody text: with a "
        "colon.\n\nStory-ID: A\nTask-ID:  B \n  folded\n"
        "Co-Authored-By: Claude <c@example.org>\n");
    REQUIRE(t.size() == 3);
    CHECK(t[0].key == "Story-ID");
    CHECK(t[0].value == "A");
    CHECK(t[0].line == 5);
    CHECK(t[1].value == "B folded");
    CHECK(t[2].key == "Co-Authored-By");

    CHECK(ops::trailers_of("Fix: a subject only\n").empty());
    CHECK(ops::trailers_of("Subject\n\nStory-ID: A\nnot a trailer\n")
              .empty());
    CHECK(ops::trailers_of("").empty());
}

TEST_CASE("the rows of a pull request's table are its cells, trimmed",
    "[ops][provenance]") {
    const auto rows = ops::markdown_rows(ore_body);
    REQUIRE(rows.size() == 4);
    CHECK(rows[0].cells == std::vector<std::string>{"Artefact", "Link",
        "ID"});
    CHECK(rows[1].line == 9);
    CHECK(rows[1].cells[2] == story);
    CHECK(rows[3].cells == std::vector<std::string>{"Environment",
        "jolly_knuth", ""});
}

TEST_CASE("complete commits and a matching pull request pass",
    "[ops][provenance]") {
    const std::vector<types::branch_commit> commits{
        commit("[a] One", complete()),
        commit("[agile] Record PR #12 on task", {
            {"Story-ID", std::string(story), 3},
            {"Task-ID", std::string(task), 4}}),
        commit("Merge branch main", {}, 2)};
    const auto r = ops::check_provenance(ore_rules(), commits, documents(),
        types::pull_request{12, "https://x/pull/12", ore_body}, "", "b");
    CHECK(r.findings.empty());
    CHECK(r.commits == 2);
    CHECK(r.merges == 1);
    CHECK(r.pull_request == 12u);
}

TEST_CASE("a commit with no trailer, a malformed one, or one naming no "
    "document of its type is reported", "[ops][provenance]") {
    const std::vector<types::branch_commit> commits{
        commit("[a] None", {}),
        commit("[a] Bad", {{"Story-ID", "e6359881-7919-4bbc-9b9c-"
            "03a8eedb8a03", 3}, {"Task-ID", std::string(story), 4},
            {"Environment", "Jolly Knuth", 5}}),
        commit("[a] Unknown", {{"Story-ID", "00000000-0000-0000-0000-"
            "000000000000", 3}, {"Task-ID", std::string(task), 4},
            {"Environment", "jolly_knuth", 5}})};
    const auto r = ops::check_provenance(ore_rules(), commits, documents(),
        std::nullopt, "gh is off", "b");
    CHECK(rules_of(r) == std::vector<std::string>{
        "commit/0123456789ab:1:commit_provenance/missing_trailer",
        "commit/0123456789ab:1:commit_provenance/missing_trailer",
        "commit/0123456789ab:1:commit_provenance/missing_trailer",
        "commit/0123456789ab:3:commit_provenance/malformed_trailer",
        "commit/0123456789ab:4:commit_provenance/wrong_document_type",
        "commit/0123456789ab:5:commit_provenance/malformed_trailer",
        "commit/0123456789ab:3:commit_provenance/unknown_document"});
    CHECK_THAT(r.findings[4].message, ContainsSubstring("names "
        "doc/story.org, a story, not a task"));
    CHECK(r.note == "gh is off");
    CHECK_FALSE(r.pull_request);
}

TEST_CASE("a commit of another task than the pull request's is a warning",
    "[ops][provenance]") {
    const std::vector<types::branch_commit> commits{
        commit("[a] Other", complete(other_task))};
    const auto r = ops::check_provenance(ore_rules(), commits, documents(),
        types::pull_request{12, "", ore_body}, "", "b");
    REQUIRE(rules_of(r) == std::vector<std::string>{
        "commit/0123456789ab:4:commit_provenance/differs_from_pull_request"});
    CHECK(r.findings[0].level == compass::codec::types::severity::warning);
    CHECK_FALSE(types::fails(r.findings));
}

TEST_CASE("a pull request's table must state each labelled trailer",
    "[ops][provenance]") {
    std::string body = ore_body;
    body.replace(body.find("| Environment | jolly_knuth | |"),
        std::string_view("| Environment | jolly_knuth | |").size(),
        "| Environment | | |");
    body.replace(body.find("| Task |"), 6, "| Job |");
    const std::vector<types::branch_commit> commits{
        commit("[a] One", complete())};
    const auto r = ops::check_provenance(ore_rules(), commits, documents(),
        types::pull_request{12, "", body}, "", "b");
    CHECK(rules_of(r) == std::vector<std::string>{
        "pull/12:1:commit_provenance/pull_request_table",
        "pull/12:11:commit_provenance/pull_request_table"});
}

TEST_CASE("a branch with no pull request fails only when one is required",
    "[ops][provenance]") {
    auto rules = ore_rules();
    const std::vector<types::branch_commit> commits{
        commit("[a] One", complete())};
    CHECK(ops::check_provenance(rules, commits, documents(), std::nullopt,
        "no pull requests found", "feature/x").findings.empty());
    rules.require_pull_request = true;
    const auto r = ops::check_provenance(rules, commits, documents(),
        std::nullopt, "no pull requests found", "feature/x");
    CHECK(rules_of(r) == std::vector<std::string>{
        "branch/feature/x:1:commit_provenance/no_pull_request"});
}

TEST_CASE("the rules come from the check document's tables",
    "[ops][provenance]") {
    compass::codec::ops::data_tables tables{
        .source = "compass/checks/commit_provenance.org", .tables = {
            {.name = "Trailers", .line = 9, .columns = {"trailer",
                 "document type", "pattern", "pull request label",
                 "pull request column"},
             .rows = {{"Story-ID", "story", "", "Story", "3"},
                 {"Environment", "", "[a-z]+_[a-z]+", "", ""}},
             .row_lines = {12, 13}},
            {.name = "Exemptions", .line = 15, .columns = {"subject",
                 "omits"},
             .rows = {{"^Merge", "Story-ID Environment"}},
             .row_lines = {18}}}};
    const auto data = compass::codec::ops::encode_data_tables(tables);
    const auto r = ops::provenance_rules_of(data, "true");
    REQUIRE(r);
    CHECK(r->require_pull_request);
    REQUIRE(r->trailers.size() == 2);
    CHECK(r->trailers[0].column == 3);
    CHECK(r->trailers[1].label.empty());
    CHECK(r->exemptions[0].omits == std::vector<std::string>{"Story-ID",
        "Environment"});

    CHECK_THAT(ops::provenance_rules_of("", "false").error(),
        ContainsSubstring("write compass/checks/commit_provenance.org"));
    CHECK_THAT(ops::provenance_rules_of(data, "yes").error(),
        ContainsSubstring("write true or false"));

    tables.tables[0].rows[1][1] = "environment";
    CHECK_THAT(ops::provenance_rules_of(
        compass::codec::ops::encode_data_tables(tables), "false").error(),
        ContainsSubstring("commit_provenance.org:13: Environment needs a "
            "document type or a pattern, and not both"));
    tables.tables[0].rows[1][1] = "";
    tables.tables[0].rows[1][2] = "([";
    CHECK_THAT(ops::provenance_rules_of(
        compass::codec::ops::encode_data_tables(tables), "false").error(),
        ContainsSubstring("does not compile"));
    tables.tables[0].rows[1][2] = "x";
    tables.tables[1].rows[0][1] = "Task-ID";
    CHECK_THAT(ops::provenance_rules_of(
        compass::codec::ops::encode_data_tables(tables), "false").error(),
        ContainsSubstring("Task-ID is no trailer of the Trailers table"));
}

TEST_CASE("gh's JSON gives the pull request", "[io][provenance]") {
    const auto pr = io::parse_pull_request(
        R"({"body":"| Story | x | y |\n","number":4966,"url":"https://u"})");
    REQUIRE(pr);
    CHECK(pr->number == 4966);
    CHECK(pr->url == "https://u");
    CHECK(pr->body == "| Story | x | y |\n");
    CHECK_FALSE(io::parse_pull_request("no pull requests found"));
    CHECK_FALSE(io::parse_pull_request(R"({"url":"https://u"})"));
}

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
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.cli/app/application.hpp"
#include "compass.platform/process/run_program.hpp"
#include "scoped_git_repo.hpp"

namespace {

using namespace compass::cli;
using tests::scoped_git_repo;
using Catch::Matchers::ContainsSubstring;

constexpr std::string_view story = "E6359881-7919-4BBC-9B9C-03A8EEDB8A03";
constexpr std::string_view task = "C6436C51-A6DB-422B-AC5E-B60255F90219";
constexpr std::string_view other_task =
    "67AF2BC8-0FD7-4382-A4B3-C2407C408708";

constexpr std::string_view configuration =
    "#+output: out\n\n* Codecs\n\n"
    "| name | enabled | options |\n"
    "| document_index | no | |\n"
    "| required_sections | no | |\n"
    "| commit_provenance | yes | |\n";

constexpr std::string_view check_document =
    "#+title: Check: commit provenance\n\n"
    "* Definition\n:PROPERTIES:\n:CODEC: commit_provenance\n:END:\n\n"
    "* Trailers\n\n"
    "| trailer | document type | pattern | pull request label "
    "| pull request column |\n"
    "|---+---+---+---+---|\n"
    "| Story-ID | story | | Story | 3 |\n"
    "| Task-ID | task | | Task | 3 |\n"
    "| Environment | | =[a-z]+_[a-z]+= | Environment | 2 |\n\n"
    "* Exemptions\n\n"
    "| subject | omits |\n"
    "|---+---|\n"
    "| =^\\[agile\\] Record PR #= | Environment |\n";

std::string document(std::string_view id, std::string_view type) {
    return ":PROPERTIES:\n:ID: " + std::string(id) + "\n:END:\n#+title: " +
        std::string(type) + "\n#+type: " + std::string(type) + "\n";
}

/**
 * @brief The trailers of a commit of @p of_task in the story, from the
 * environment jolly_knuth.
 */
std::string trailers(std::string_view of_task = task) {
    return "Story-ID: " + std::string(story) + "\nTask-ID: " +
        std::string(of_task) + "\nEnvironment: jolly_knuth\n";
}

void commit(const scoped_git_repo& repo, std::string_view message) {
    const auto file = repo.output_root() / "message.txt";
    scoped_git_repo::write_file(file, message);
    repo.git("add -A");
    repo.git("-c user.name=t -c user.email=t@example.org "
             "-c commit.gpgsign=false commit -q --allow-empty -F '" +
        file.generic_string() + "'");
}

/**
 * @brief A repository whose branch base holds the story, two tasks and
 * the check document, and whose HEAD is on the branch feature/x made
 * from it.
 */
void make_base(const scoped_git_repo& repo,
    std::string_view config = configuration) {
    repo.write(".compass.org", config);
    repo.write("compass/checks/commit_provenance.org", check_document);
    repo.write("doc/story.org", document(story, "story"));
    repo.write("doc/task.org", document(task, "task"));
    repo.write("doc/other_task.org", document(other_task, "task"));
    commit(repo, "Base\n");
    repo.git("branch base");
    repo.git("checkout -q -b feature/x");
}

/**
 * @brief A program that stands in for gh: it prints @p output and exits
 * with @p status, whatever it is asked.
 */
std::string fake_gh(const scoped_git_repo& repo, std::string_view output,
    int status = 0) {
    const auto data = repo.output_root() / "gh.out";
    scoped_git_repo::write_file(data, output);
#ifdef _WIN32
    const auto script = repo.output_root() / "gh.cmd";
    scoped_git_repo::write_file(script, "@type \"" + data.string() +
        "\"\r\n@exit /b " + std::to_string(status) + "\r\n");
#else
    const auto script = repo.output_root() / "gh";
    scoped_git_repo::write_file(script, "#!/bin/sh\ncat '" +
        data.generic_string() + "'\nexit " + std::to_string(status) + "\n");
    std::filesystem::permissions(script,
        std::filesystem::perms::owner_all);
#endif
    return script.generic_string();
}

/**
 * @brief The JSON gh prints for pull request 7 with a table of @p of_task
 * in the story.
 */
std::string pull_request(std::string_view of_task = task) {
    return R"({"number":7,"url":"https://example.org/pull/7","body":)"
        R"("## Traceability\n\n| Artefact | Link | ID |\n|---|---|---|\n)"
        R"(| Story | [s](https://example.org/s) | )" + std::string(story) +
        R"( |\n| Task | [t](https://example.org/t) | )" +
        std::string(of_task) + R"( |\n| Environment | jolly_knuth | |\n"})";
}

struct outcome {
    int status = -1;
    std::string out;
    std::string err;

    /**
     * @brief Each finding of out, as <path>:<line>:<rule>, a warning's
     * rule marked so.
     */
    [[nodiscard]] std::vector<std::string> findings() const {
        static const std::regex finding(
            "^(\\S+):([0-9]+):[0-9]+: (warning: )?(\\S+): .*");
        std::vector<std::string> lines;
        std::istringstream in(out);
        std::smatch m;
        for (std::string line; std::getline(in, line);) {
            if (std::regex_match(line, m, finding))
                lines.push_back(std::string(m[1]) + ":" + std::string(m[2]) +
                    ":" + (m[3].matched ? "warning " : "") +
                    std::string(m[4]));
        }
        return lines;
    }
};

outcome run(const scoped_git_repo& repo, std::vector<std::string> extra,
    std::string_view gh = "") {
    std::vector<std::string> args{"check", "-C",
        repo.checkout().generic_string(), "-j", "2",
        "--set", "commit_provenance.gh=" + std::string(gh)};
    args.insert(args.end(), extra.begin(), extra.end());
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

outcome ratchet(const scoped_git_repo& repo, std::string_view gh = "",
    std::vector<std::string> extra = {}) {
    extra.insert(extra.begin(), {"--ratchet", "base"});
    return run(repo, std::move(extra), gh);
}

std::string head_id(const scoped_git_repo& repo) {
    auto id = compass::platform::process::run_program({{"git", "-C",
        repo.checkout().generic_string(), "rev-parse", "--short=12", "HEAD"},
        {}, {}}).output;
    while (!id.empty() && id.back() == '\n')
        id.pop_back();
    return id;
}

}

TEST_CASE("a branch whose commits state their work passes",
    "[app][provenance]") {
    scoped_git_repo repo;
    make_base(repo);
    commit(repo, "[a] One\n\nWhy.\n\n" + trailers());
    commit(repo, "[agile] Record PR #7 on task\n\nStory-ID: " +
        std::string(story) + "\nTask-ID: " + std::string(task) + "\n");

    const auto r = ratchet(repo);
    CHECK(r.status == 0);
    CHECK(r.findings().empty());
    CHECK_THAT(r.out, ContainsSubstring("commit provenance: 2 commits "
        "checked, 0 merges skipped; no pull request checked: gh is off"));
}

TEST_CASE("trailers set apart from a closing Co-Authored-By by a blank line "
    "still count", "[app][provenance]") {
    scoped_git_repo repo;
    make_base(repo);
    commit(repo, "[a] One\n\nWhy.\n\n" + trailers() +
        "\nCo-Authored-By: A Person <a@example.com>\n");

    const auto r = ratchet(repo);
    CHECK(r.status == 0);
    CHECK(r.findings().empty());
}

TEST_CASE("a commit with no trailer fails the ratchet", "[app][provenance]") {
    scoped_git_repo repo;
    make_base(repo);
    commit(repo, "[a] One\n\n" + trailers());
    commit(repo, "[a] Two\n\nTask-ID: " + std::string(task) +
        "\nEnvironment: jolly_knuth\n");

    const auto r = ratchet(repo);
    CHECK(r.status == 1);
    CHECK(r.findings() == std::vector<std::string>{"commit/" + head_id(repo) +
        ":1:commit_provenance/missing_trailer"});
    CHECK_THAT(r.out, ContainsSubstring("the commit \"[a] Two\" has no "
        "Story-ID trailer"));
}

TEST_CASE("a malformed UUID fails the ratchet", "[app][provenance]") {
    scoped_git_repo repo;
    make_base(repo);
    commit(repo, "[a] One\n\nStory-ID: E6359881\nTask-ID: " +
        std::string(task) + "\nEnvironment: jolly_knuth\n");

    const auto r = ratchet(repo);
    CHECK(r.status == 1);
    CHECK(r.findings() == std::vector<std::string>{"commit/" + head_id(repo) +
        ":3:commit_provenance/malformed_trailer"});
}

TEST_CASE("an ID that names no document, or one of the wrong type, fails "
    "the ratchet", "[app][provenance]") {
    scoped_git_repo repo;
    make_base(repo);
    commit(repo, "[a] One\n\nStory-ID: " + std::string(task) +
        "\nTask-ID: 00000000-0000-0000-0000-000000000000\n"
        "Environment: jolly_knuth\n");

    const auto r = ratchet(repo);
    CHECK(r.status == 1);
    const auto id = head_id(repo);
    CHECK(r.findings() == std::vector<std::string>{
        "commit/" + id + ":3:commit_provenance/wrong_document_type",
        "commit/" + id + ":4:commit_provenance/unknown_document"});
    CHECK_THAT(r.out, ContainsSubstring("names doc/task.org, a task, not a "
        "story"));
}

TEST_CASE("merge commits are skipped", "[app][provenance]") {
    scoped_git_repo repo;
    make_base(repo);
    repo.git("checkout -q -b side");
    commit(repo, "[a] Side\n\n" + trailers());
    repo.git("checkout -q feature/x");
    commit(repo, "[a] One\n\n" + trailers());
    repo.git("-c user.name=t -c user.email=t@example.org "
             "-c commit.gpgsign=false merge -q --no-ff -m 'Merge side' side");

    const auto r = ratchet(repo);
    CHECK(r.status == 0);
    CHECK(r.findings().empty());
    CHECK_THAT(r.out, ContainsSubstring("2 commits checked, 1 merge "
        "skipped"));
}

TEST_CASE("the pull request's table is read and matched with the commits",
    "[app][provenance]") {
    scoped_git_repo repo;
    make_base(repo);
    commit(repo, "[a] One\n\n" + trailers());

    SECTION("a table that matches") {
        const auto r = ratchet(repo, fake_gh(repo, pull_request()));
        CHECK(r.status == 0);
        CHECK(r.findings().empty());
        CHECK_THAT(r.out, ContainsSubstring("1 commit checked, 0 merges "
            "skipped; pull request #7 checked"));
    }
    SECTION("a table of another task") {
        const auto r = ratchet(repo, fake_gh(repo, pull_request(other_task)));
        CHECK(r.status == 0);
        CHECK(r.findings() == std::vector<std::string>{"commit/" +
            head_id(repo) + ":4:warning commit_provenance/"
            "differs_from_pull_request"});
    }
}

TEST_CASE("a branch with no pull request says so, and fails only when one "
    "is required", "[app][provenance]") {
    scoped_git_repo repo;
    make_base(repo);
    commit(repo, "[a] One\n\n" + trailers());
    const auto gh = fake_gh(repo,
        "no pull requests found for branch \"feature/x\"\n", 1);

    const auto lenient = ratchet(repo, gh);
    CHECK(lenient.status == 0);
    CHECK_THAT(lenient.out, ContainsSubstring("no pull request checked: " +
        gh + ": no pull requests found for branch "
        "\"feature/x\""));

    const auto strict = ratchet(repo, gh,
        {"--set", "commit_provenance.require_pull_request=true"});
    CHECK(strict.status == 1);
    CHECK(strict.findings() == std::vector<std::string>{
        "branch/feature/x:1:commit_provenance/no_pull_request"});
}

TEST_CASE("a check without its check document reports it",
    "[app][provenance]") {
    scoped_git_repo repo;
    make_base(repo);
    repo.git("rm -q compass/checks/commit_provenance.org");
    commit(repo, "[a] One\n\n" + trailers());

    const auto plain = run(repo, {});
    CHECK(plain.status == 1);
    CHECK(plain.findings() == std::vector<std::string>{
        "compass/checks/commit_provenance.org:1:"
        "commit_provenance/unreadable"});

    const auto r = ratchet(repo);
    CHECK_THAT(r.out, ContainsSubstring("commit provenance: not checked: "
        "commit_provenance reads its data from a check document"));
}

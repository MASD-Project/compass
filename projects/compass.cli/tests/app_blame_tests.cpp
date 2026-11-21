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
#include "scoped_git_repo.hpp"

namespace {

using namespace compass::cli;
using tests::scoped_git_repo;
using Catch::Matchers::ContainsSubstring;

constexpr std::string_view story = "E6359881-7919-4BBC-9B9C-03A8EEDB8A03";
constexpr std::string_view task = "C6436C51-A6DB-422B-AC5E-B60255F90219";

constexpr std::string_view configuration =
    "#+output: out\n\n* Codecs\n\n"
    "| name | enabled | options |\n"
    "| document_index | no | |\n"
    "| required_sections | no | |\n";

constexpr std::string_view provenance_document =
    "#+title: Check: commit provenance\n\n"
    "* Definition\n:PROPERTIES:\n:CODEC: commit_provenance\n:END:\n\n"
    "* Trailers\n\n"
    "| trailer | document type | pattern | pull request label "
    "| pull request column |\n"
    "|---+---+---+---+---|\n"
    "| Story-ID | story | | Story | 3 |\n"
    "| Task-ID | task | | Task | 3 |\n"
    "| Environment | | =[a-z]+_[a-z]+= | Environment | 2 |\n";

constexpr std::string_view forbid_document =
    "#+title: Check: portable C++\n\n"
    "* Definition\n"
    ":PROPERTIES:\n"
    ":ENGINE: forbid\n"
    ":FILES: src/**/*.cpp\n"
    ":END:\n\n"
    "** atomic_shared_ptr\n"
    ":PROPERTIES:\n"
    ":CPP_TEXT: \\batomic\\s*<\\s*(std\\s*::\\s*)?shared_ptr\\b\n"
    ":MESSAGE: std::atomic<std::shared_ptr> is not portable\n"
    ":END:\n\n"
    "Apple's libc++ lacks it.\n";

constexpr std::string_view limit_document =
    "#+title: Check: wide types\n\n"
    "* Definition\n"
    ":PROPERTIES:\n"
    ":ENGINE: limit\n"
    ":FILES: src/**/*.hpp\n"
    ":FACT: cpp_fields\n"
    ":CEILING: 2\n"
    ":END:\n\n"
    "* Ceilings\n\n"
    "| path | type | ceiling |\n"
    "|------+------+---------|\n";

constexpr std::string_view violation =
    "std::atomic<std::shared_ptr<int>> current;\n";

std::string trailers(std::string_view environment) {
    return "\n\nStory-ID: " + std::string(story) + "\nTask-ID: " +
        std::string(task) + "\nEnvironment: " + std::string(environment) +
        "\n";
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
 * @brief Merges the branch @p branch into main as GitHub merges pull
 * request @p number.
 */
void merge(const scoped_git_repo& repo, std::string_view branch,
    int number) {
    repo.git("checkout -q main");
    repo.git("-c user.name=t -c user.email=t@example.org "
             "-c commit.gpgsign=false merge -q --no-ff -m 'Merge pull "
             "request #" + std::to_string(number) + " from o/" +
        std::string(branch) + "' " + std::string(branch));
}

/**
 * @brief A main branch that holds the configuration, the story, the task
 * and the check documents of @p checks, and no finding.
 */
void make_main(const scoped_git_repo& repo,
    std::string_view config = configuration) {
    repo.git("checkout -q -b main");
    repo.write(".compass.org", config);
    repo.write("compass/checks/commit_provenance.org", provenance_document);
    repo.write("compass/checks/portable_cpp.org", forbid_document);
    repo.write("doc/story.org", ":PROPERTIES:\n:ID: " + std::string(story) +
        "\n:END:\n#+title: Story: trace a break\n#+type: story\n");
    repo.write("doc/task.org", ":PROPERTIES:\n:ID: " + std::string(task) +
        "\n:END:\n#+title: Task: blame a finding\n#+type: task\n");
    repo.write("src/core/clean.cpp", "int a;\n");
    commit(repo, "Base\n");
}

/**
 * @brief Commits @p content to @p path on a branch from main, with
 * @p message, and merges it as pull request @p number.
 */
void land(const scoped_git_repo& repo, std::string_view path,
    std::string_view content, std::string_view message, int number) {
    const auto branch = "feature/pr" + std::to_string(number);
    repo.git("checkout -q -b " + branch + " main");
    repo.write(std::string(path), content);
    commit(repo, message);
    merge(repo, branch, number);
}

struct outcome {
    int status = -1;
    std::string out;
    std::string err;
};

outcome run(const scoped_git_repo& repo, std::vector<std::string> args) {
    args.insert(args.begin() + 1, {"-C", repo.checkout().generic_string(),
        "-j", "2"});
    if (args.front() != "generate")
        args.insert(args.begin() + 1, {"--today", "2026-11-18"});
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

}

TEST_CASE("check blames a finding on main by its line, naming the commit, "
    "pull request, task, story and environment", "[app][blame]") {
    scoped_git_repo repo;
    make_main(repo);
    land(repo, "src/core/cache.cpp", "int a;\n" + std::string(violation),
        "[core] Cache the current value" + trailers("jolly_knuth"), 4966);

    const auto r = run(repo, {"check"});
    CHECK(r.status == 1);
    CHECK_THAT(r.out, ContainsSubstring("compass blame: 1 finding already "
        "on main, traced to 1 environment"));
    CHECK_THAT(r.out, ContainsSubstring("Tell environment jolly_knuth that "
        "it broke them"));
    CHECK_THAT(r.out, ContainsSubstring("- portable_cpp/atomic_shared_ptr "
        "at src/core/cache.cpp:2:"));
    CHECK_THAT(r.out, ContainsSubstring("'[core] Cache the current value' "
        "in PR #4966"));
    CHECK_THAT(r.out, ContainsSubstring("Task-ID C6436C51 'Task: blame a "
        "finding'"));
    CHECK_THAT(r.out, ContainsSubstring("Story-ID E6359881 'Story: trace a "
        "break'"));
    CHECK_THAT(r.out, ContainsSubstring("Environment jolly_knuth"));
    CHECK_THAT(r.out, ContainsSubstring("found by: line blame"));
    CHECK_THAT(r.out, ContainsSubstring("  - fix: "));

    const auto quiet = run(repo, {"check", "--no-blame"});
    CHECK_THAT(quiet.out, !ContainsSubstring("compass blame"));
}

TEST_CASE("check sends one message per environment, and names the author "
    "and pull request when the commit states none", "[app][blame]") {
    scoped_git_repo repo;
    make_main(repo);
    land(repo, "src/core/a.cpp", violation,
        "[core] Add a" + trailers("jolly_knuth"), 10);
    land(repo, "src/core/b.cpp", violation,
        "[core] Add b" + trailers("brave_hopper"), 11);
    land(repo, "src/core/c.cpp", violation, "[core] Add c\n", 12);

    const auto r = run(repo, {"check"});
    const auto brave = r.out.find("Tell environment brave_hopper");
    const auto jolly = r.out.find("Tell environment jolly_knuth");
    const auto none = r.out.find("state no Environment trailer");
    REQUIRE(brave != std::string::npos);
    REQUIRE(jolly != std::string::npos);
    REQUIRE(none != std::string::npos);
    CHECK(brave < jolly);
    CHECK(jolly < none);
    CHECK(r.out.find("src/core/b.cpp", brave) < jolly);
    CHECK(r.out.find("src/core/a.cpp", jolly) < none);
    const auto unnamed = r.out.substr(none);
    CHECK_THAT(unnamed, ContainsSubstring("src/core/c.cpp"));
    CHECK_THAT(unnamed, ContainsSubstring("'[core] Add c' in PR #12, by t, "
        "Story-ID not stated, Task-ID not stated, Environment not stated"));
}

TEST_CASE("check blames a missing section on the commit that added the "
    "document, as a guess", "[app][blame]") {
    scoped_git_repo repo;
    make_main(repo, "#+output: out\n\n* Codecs\n\n"
        "| name | enabled | options |\n"
        "| document_index | no | |\n");
    std::filesystem::remove(repo.checkout() / "doc" / "task_b.org");
    commit(repo, "Drop task b\n");
    land(repo, "doc/task_b.org", "#+title: Task: b\n#+type: task\n* Goal\n",
        "[agile] Add task b" + trailers("eager_maxwell"), 20);

    const auto r = run(repo, {"check"});
    CHECK_THAT(r.out, ContainsSubstring("Tell environment eager_maxwell that "
        "it likely broke them"));
    CHECK_THAT(r.out, ContainsSubstring("- required_sections/missing_section "
        "at doc/task_b.org:"));
    CHECK_THAT(r.out, ContainsSubstring("'[agile] Add task b' in PR #20"));
    CHECK_THAT(r.out, ContainsSubstring("found by: added blame, likely: the "
        "commit that added doc/task_b.org"));
}

TEST_CASE("check blames drift on the commit that edited the generated file "
    "by hand", "[app][blame]") {
    scoped_git_repo repo;
    make_main(repo, "#+output: .\n\n* Codecs\n\n"
        "| name | enabled | options |\n"
        "| required_sections | no | |\n");
    const auto generated = run(repo, {"generate"});
    INFO(generated.err);
    REQUIRE(generated.status == 0);
    commit(repo, "Generate the index" + trailers("clever_dijkstra"));
    const auto index = repo.checkout() / "doc" / "document_index.tsv";
    land(repo, "doc/document_index.tsv",
        scoped_git_repo::read_file(index) + "hand\tedit\n",
        "[doc] Fix the index by hand" + trailers("solid_dirac"), 30);

    const auto r = run(repo, {"check"});
    CHECK_THAT(r.out, ContainsSubstring("- document_index/drift at "
        "doc/document_index.tsv:"));
    CHECK_THAT(r.out, ContainsSubstring("Tell environment solid_dirac that "
        "it likely broke them"));
    CHECK_THAT(r.out, ContainsSubstring("  - hand edit: commit "));
    CHECK_THAT(r.out, ContainsSubstring("'[doc] Fix the index by hand' in "
        "PR #30"));
    CHECK_THAT(r.out, ContainsSubstring("found by: drift blame, likely"));

    land(repo, "doc/zeta.org", ":PROPERTIES:\n:ID: Z-1\n:END:\n"
        "#+title: Zeta\n", "[doc] Add zeta" + trailers("merry_newton"), 31);
    const auto later = run(repo, {"check"});
    CHECK_THAT(later.out, ContainsSubstring("  - input changed without "
        "regenerating: commit "));
    CHECK_THAT(later.out, ContainsSubstring("'[doc] Add zeta' in PR #31"));
    CHECK_THAT(later.out, ContainsSubstring("inputs it changed: "
        "doc/zeta.org"));
}

TEST_CASE("check --blame=bisect finds the merge and the commit that brought "
    "a finding no line holds", "[app][blame]") {
    scoped_git_repo repo;
    make_main(repo);
    repo.write("compass/checks/wide_types.org", limit_document);
    repo.write("src/a.hpp", "struct narrow { int a; int b; };\n");
    commit(repo, "Add the limit check\n");
    land(repo, "doc/notes.org", "#+title: Notes\n", "[doc] Notes\n", 40);
    land(repo, "src/a.hpp", "struct narrow { int a; int b; };\n"
        "struct wide {\n    int a, b, c;\n};\n",
        "[core] Widen" + trailers("merry_newton"), 41);
    land(repo, "doc/more.org", "#+title: More\n", "[doc] More\n", 42);

    const auto cheap = run(repo, {"check"});
    CHECK_THAT(cheap.out, ContainsSubstring("- wide_types/over_ceiling at "
        "src/a.hpp:2: bisect, likely"));
    CHECK_THAT(cheap.out, ContainsSubstring("check --blame=bisect would "
        "run it"));

    const auto r = run(repo, {"check", "--blame=bisect"});
    CHECK_THAT(r.out, ContainsSubstring("Tell environment merry_newton that "
        "it likely broke them"));
    CHECK_THAT(r.out, ContainsSubstring("'[core] Widen' in PR #41"));
    CHECK_THAT(r.out, ContainsSubstring("found by: bisect, likely"));
    CHECK_THAT(r.out, ContainsSubstring("bisect ran "));
}

TEST_CASE("compass blame traces one finding, the branch's own included",
    "[app][blame]") {
    scoped_git_repo repo;
    make_main(repo);
    repo.git("checkout -q -b feature/mine main");
    repo.write("src/core/mine.cpp", violation);
    commit(repo, "[core] Mine" + trailers("swift_curie"));

    const auto r = run(repo, {"blame",
        "src/core/mine.cpp:1:portable_cpp/atomic_shared_ptr"});
    INFO(r.err);
    CHECK(r.status == 0);
    CHECK_THAT(r.out, ContainsSubstring("0 findings already on main"));
    CHECK_THAT(r.out, ContainsSubstring("this branch's own to fix"));
    CHECK_THAT(r.out, ContainsSubstring("'[core] Mine' in no pull request"));

    const auto missing = run(repo, {"blame",
        "src/core/clean.cpp:1:portable_cpp/atomic_shared_ptr"});
    CHECK(missing.status == 2);
    const auto bad = run(repo, {"blame", "src/core/mine.cpp"});
    CHECK(bad.status == 2);
}

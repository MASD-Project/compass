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
#include "compass.cli/app/application.hpp"

#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "scoped_git_repo.hpp"

namespace {

using compass::cli::tests::scoped_git_repo;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;

struct outcome {
    int status = -1;
    std::string out;
    std::string err;
};

outcome run(std::vector<std::string> args,
    const std::filesystem::path& working_directory = "/") {
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::ostringstream out;
    std::ostringstream err;
    const auto status =
        compass::cli::app::run(views, working_directory, out, err);
    return {status, out.str(), err.str()};
}

outcome run_on(const scoped_git_repo& repo, const std::string& command) {
    return run({command, "-C", repo.checkout().string(), "--out",
        repo.output_root().string(), "-j", "2"});
}

}

TEST_CASE("generate then diff exits 0 and prints nothing on standard "
    "output", "[app][application]") {
    scoped_git_repo repo;

    const auto generated = run_on(repo, "generate");
    CHECK(generated.status == 0);
    CHECK_THAT(generated.out, StartsWith("1 written, 0 unchanged, 0 restored, "
        "0 removed, 0 kept under "));
    CHECK(generated.err.empty());

    const auto diffed = run_on(repo, "diff");
    CHECK(diffed.status == 0);
    CHECK(diffed.out.empty());
    CHECK_THAT(diffed.err, StartsWith("0 to write, 1 unchanged"));
}

TEST_CASE("a hand edit makes diff exit 1 with its diff and gen restore it",
    "[app][application]") {
    scoped_git_repo repo;
    REQUIRE(run_on(repo, "gen").status == 0);
    const auto index = repo.output_root() / "doc" / "document_index.tsv";
    const auto generated = scoped_git_repo::read_file(index);
    scoped_git_repo::write_file(index, "edited by hand\n");

    const auto diffed = run_on(repo, "diff");
    CHECK(diffed.status == 1);
    CHECK_THAT(diffed.out, ContainsSubstring("-edited by hand") &&
        ContainsSubstring("+A-1\tTask: a\ttask\tdoc/task_a.org"));
    CHECK(scoped_git_repo::read_file(index) == "edited by hand\n");

    const auto restored = run_on(repo, "gen");
    CHECK(restored.status == 0);
    CHECK_THAT(restored.out, StartsWith("0 written, 0 unchanged, 1 restored"));
    CHECK(scoped_git_repo::read_file(index) == generated);
    CHECK(run_on(repo, "diff").status == 0);
}

TEST_CASE("check reports a missing section with path, position and fix and "
    "exits 1, then 0 once the section is there", "[app][application]") {
    scoped_git_repo repo;

    const auto missing = run_on(repo, "check");
    CHECK(missing.status == 1);
    CHECK_THAT(missing.out, StartsWith(
        "doc/task_b.org:2:9: required_sections/missing_section: task "
        "document has no \"Result\" section (fix: Add the heading "
        "\"* Result\"; the task contract requires it.)\n1 diagnostic ("));

    repo.write("doc/task_b.org",
        "#+title: Task: b\n#+type: task\n* Goal\n* Result\n");
    const auto fixed = run_on(repo, "check");
    CHECK(fixed.status == 0);
    CHECK_THAT(fixed.out, StartsWith("0 diagnostics ("));
}

TEST_CASE("bad usage exits 2 and says why", "[app][application]") {
    const auto no_command = run({});
    CHECK(no_command.status == 2);
    CHECK_THAT(no_command.err, StartsWith("compass: no command\n"));

    const auto unknown = run({"write"});
    CHECK(unknown.status == 2);
    CHECK_THAT(unknown.err, StartsWith("compass: unknown command: write\n"));

    const auto gc_only = run({"check", "--dry-run"});
    CHECK(gc_only.status == 2);
    CHECK_THAT(gc_only.err,
        StartsWith("compass: --dry-run applies to gc only\n"));

    const auto missing_value = run({"check", "-j"});
    CHECK(missing_value.status == 2);
    CHECK(missing_value.out.empty());
}

TEST_CASE("a checkout that is not a git repository fails with 2",
    "[app][application]") {
    const auto r = run({"check", "-C", "/nonexistent/compass/checkout"});
    CHECK(r.status == 2);
    CHECK_THAT(r.err, StartsWith("compass: "));
}

TEST_CASE("help and version exit 0 on standard output", "[app][application]") {
    const auto help = run({"gc", "--help"});
    CHECK(help.status == 0);
    CHECK_THAT(help.out, ContainsSubstring("generate, gen"));

    const auto version = run({"--version"});
    CHECK(version.status == 0);
    CHECK_THAT(version.out, StartsWith("compass "));
}

TEST_CASE("config prints every value with where it came from",
    "[app][application]") {
    scoped_git_repo repo;
    repo.write(".compass.org",
        "#+title: Compass configuration\n#+output: out\n\n* Codecs\n\n"
        "| name | enabled | options |\n|-\n"
        "| document_index | yes | path=index.tsv |\n");

    const auto r = run({"config", "-C", repo.checkout().string(), "--set",
        "required_sections.enabled=false"});

    CHECK(r.status == 0);
    CHECK(r.err.empty());
    CHECK(r.out ==
        "output = out (.compass.org:2)\n"
        "meta = doc/meta (default)\n"
        "document_index.enabled = true (.compass.org:8)\n"
        "document_index.path = index.tsv (.compass.org:8)\n"
        "required_sections.enabled = false (command line)\n"
        "tangle.enabled = false (default)\n"
        "tangle_check.enabled = false (default)\n");
}

TEST_CASE("an error in the configuration file exits 2 naming the line and "
    "the fix", "[app][application]") {
    scoped_git_repo repo;
    repo.write(".compass.org",
        "* Codecs\n| document_index | yes | colour=red |\n");

    for (const auto* command : {"check", "config"}) {
        const auto r = run({command, "-C", repo.checkout().string()});
        CHECK(r.status == 2);
        CHECK(r.out.empty());
        CHECK(r.err == "compass: .compass.org:2: codec 'document_index' has "
            "no option 'colour'; fix: use one of path\n");
    }

    repo.write(".compass.org", "* Codecs\n| html | maybe | |\n");
    const auto r = run({"gen", "-C", repo.checkout().string()});
    CHECK(r.status == 2);
    CHECK(r.err == "compass: .compass.org:2: enabled is 'maybe'; fix: write "
        "yes or no\n");
}

TEST_CASE("the configuration file sets the output root and which codecs "
    "run, and is not a document", "[app][application]") {
    scoped_git_repo repo;
    repo.write(".compass.org", "#+output: generated\n* Codecs\n"
        "| required_sections | no | |\n");

    const auto checked = run({"check", "-C", repo.checkout().string()});
    CHECK(checked.status == 0);
    CHECK_THAT(checked.out, StartsWith("0 diagnostics (4 documents, "));

    const auto generated = run({"gen", "-C", repo.checkout().string()});
    CHECK(generated.status == 0);
    CHECK(std::filesystem::exists(
        repo.checkout() / "generated" / "doc" / "document_index.tsv"));
}

TEST_CASE("editing one codec's option in the file runs only that codec's "
    "units again", "[app][application]") {
    scoped_git_repo repo;
    const auto configure = [&](std::string_view path) {
        repo.write(".compass.org", "* Codecs\n"
            "| document_index | yes | path=" + std::string(path) + " |\n"
            "| required_sections | yes | |\n");
    };
    configure("first.tsv");
    REQUIRE(run_on(repo, "gen").status == 0);
    const auto again = run_on(repo, "gen");
    CHECK_THAT(again.out, ContainsSubstring(
        "units run: document_index 0, required_sections 0, tangle 0, "
        "tangle_check 0;"));

    configure("second.tsv");
    const auto edited = run_on(repo, "gen");

    CHECK(edited.status == 0);
    CHECK_THAT(edited.out, StartsWith("1 written, 0 unchanged, 0 restored, "
        "1 removed"));
    CHECK_THAT(edited.out, ContainsSubstring(
        "units run: document_index 1, required_sections 0, tangle 0, "
        "tangle_check 0;"));
    CHECK(std::filesystem::exists(repo.output_root() / "second.tsv"));
    CHECK_FALSE(std::filesystem::exists(repo.output_root() / "first.tsv"));
}

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
#include "compass.cli/app/pipeline.hpp"

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.cli/app/application.hpp"
#include "compass.cli/io/listing.hpp"
#include "compass.codec/io/dependency_file.hpp"
#include "compass.store/io/location.hpp"
#include "scoped_git_repo.hpp"

namespace {

using namespace compass::cli;
using tests::scoped_git_repo;
using Catch::Matchers::ContainsSubstring;

void commit_all(const scoped_git_repo& repo) {
    repo.git("add -A");
    repo.git("-c user.name=t -c user.email=t@example.org "
             "-c commit.gpgsign=false commit -q -m c");
}

/**
 * @brief Options over @p repo whose output root is under the checkout, so
 * a commit can hold the outputs.
 */
types::options options_of(const scoped_git_repo& repo) {
    types::options o;
    o.which = types::command::check;
    o.checkout = repo.checkout();
    o.meta = repo.checkout() / "doc" / "meta";
    o.output_root = repo.checkout() / "out";
    o.threads = 2;
    return o;
}

types::options at_head(const scoped_git_repo& repo) {
    auto o = options_of(repo);
    o.commit = io::resolve_commit(repo.checkout(), "HEAD");
    return o;
}

/**
 * @brief Each finding as one line, as compass check prints it.
 */
std::vector<std::string> lines_of(const types::check_result& r) {
    std::vector<std::string> out;
    for (const auto& f : r.findings)
        out.push_back(f.path + ":" + std::to_string(f.line) + ":" +
            std::to_string(f.column) + ": " + f.rule + ": " + f.message);
    return out;
}

struct outcome {
    int status = -1;
    std::string out;
    std::string err;
};

outcome run(const scoped_git_repo& repo, std::vector<std::string> extra) {
    std::vector<std::string> args{"check", "-C",
        repo.checkout().generic_string(), "-j", "2"};
    args.insert(args.end(), extra.begin(), extra.end());
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

}

TEST_CASE("a check of a commit reports the commit, not the files on disk",
    "[app][commit]") {
    scoped_git_repo repo;
    commit_all(repo);
    const auto committed = lines_of(app::check(options_of(repo)));
    REQUIRE_FALSE(committed.empty());

    repo.write("doc/task_b.org",
        "#+title: Task: b\n#+type: task\n* Goal\n* Result\n");
    repo.write("doc/task_a.org", "#+title: Task: a\n#+type: task\n");
    repo.write("doc/task_c.org", "#+title: Task: c\n#+type: task\n");
    REQUIRE(lines_of(app::check(options_of(repo))) != committed);

    const auto r = app::check(at_head(repo));
    CHECK_FALSE(r.run.failed());
    CHECK(r.run.files == 4);
    CHECK(lines_of(r) == committed);
}

TEST_CASE("a check of a commit parses no document the store holds and "
    "leaves the checkout's records alone", "[app][commit]") {
    scoped_git_repo repo;
    commit_all(repo);
    const auto git_dir = compass::store::io::git_dir(repo.checkout());

    const auto cold = app::check(at_head(repo));
    CHECK(cold.run.parsed == 4);
    CHECK_FALSE(std::filesystem::exists(
        compass::store::io::worktree_index_path(git_dir)));
    CHECK_FALSE(std::filesystem::exists(
        compass::codec::io::dependency_file_path(git_dir)));

    const auto warm = app::check(at_head(repo));
    CHECK(warm.run.loaded == 4);
    CHECK(warm.run.parsed == 0);
    CHECK(lines_of(warm) == lines_of(cold));

    repo.write("doc/task_d.org", "#+title: Task: d\n#+type: task\n");
    commit_all(repo);
    const auto next = app::check(at_head(repo));
    CHECK(next.run.loaded == 4);
    CHECK(next.run.parsed == 1);
}

TEST_CASE("a check of a commit compares the outputs with the commit's "
    "files", "[app][commit]") {
    scoped_git_repo repo;
    REQUIRE(app::generate(options_of(repo)).sink.written == 1);
    const auto index = repo.checkout() / "out" / "doc" / "document_index.tsv";
    const auto generated = scoped_git_repo::read_file(index);
    commit_all(repo);

    const auto rule_is_drift = [](const types::check_result& r) {
        std::vector<std::string> out;
        for (const auto& f : r.findings)
            if (f.rule == "document_index/drift")
                out.push_back(f.path + ":" + std::to_string(f.line) + ":" +
                    std::to_string(f.column) + ": " + f.message);
        return out;
    };
    CHECK(rule_is_drift(app::check(at_head(repo))).empty());

    const auto newline = generated.find('\n');
    REQUIRE(newline != std::string::npos);
    auto edited = generated;
    edited.insert(newline + 1, "x");
    repo.write("out/doc/document_index.tsv", edited);
    commit_all(repo);
    repo.write("out/doc/document_index.tsv", generated);
    CHECK(rule_is_drift(app::check(at_head(repo))) ==
        std::vector<std::string>{"out/doc/document_index.tsv:2:1: the file "
            "differs from what document_index generates"});

    repo.git("rm -q -f out/doc/document_index.tsv");
    commit_all(repo);
    CHECK(rule_is_drift(app::check(at_head(repo))) ==
        std::vector<std::string>{"out/doc/document_index.tsv:1:1: "
            "document_index generates this file and it is missing"});
}

TEST_CASE("compass check --commit reads the configuration of the commit",
    "[app][commit]") {
    scoped_git_repo repo;
    repo.write(".compass.org", "#+output: out\n\n* Codecs\n\n"
        "| name | enabled | options |\n"
        "| required_sections | no | |\n");
    commit_all(repo);
    std::filesystem::remove(repo.checkout() / ".compass.org");

    const auto on_disk = run(repo, {});
    CHECK_THAT(on_disk.out, ContainsSubstring("required_sections/"));

    const auto at_commit = run(repo, {"--commit", "HEAD"});
    CHECK(at_commit.status == 1);
    CHECK_THAT(at_commit.out, !ContainsSubstring("required_sections/"));
    CHECK_THAT(at_commit.out, ContainsSubstring("document_index/drift"));
}

TEST_CASE("compass check --commit of a commit with no configuration warns "
    "and uses the working tree's", "[app][commit]") {
    scoped_git_repo repo;
    commit_all(repo);
    repo.write(".compass.org", "#+output: out\n\n* Codecs\n\n"
        "| name | enabled | options |\n"
        "| required_sections | no | |\n");

    const auto at_commit = run(repo, {"--commit", "HEAD"});

    CHECK_THAT(at_commit.err, ContainsSubstring("compass: warning: the "
        "commit HEAD has no .compass.org; using the working tree's"));
    CHECK_THAT(at_commit.out, !ContainsSubstring("required_sections/"));
    CHECK_THAT(at_commit.out, ContainsSubstring("document_index/drift"));
    const auto on_disk = run(repo, {});
    CHECK_THAT(on_disk.err, !ContainsSubstring("warning"));
}

TEST_CASE("compass check --commit fails on a revision that names no "
    "commit", "[app][commit]") {
    scoped_git_repo repo;
    commit_all(repo);
    const auto head = run(repo, {"--commit", "HEAD"});
    CHECK(head.status == 1);

    const auto r = run(repo, {"--commit", "no-such-branch"});
    CHECK(r.status == 2);
    CHECK_THAT(r.err, ContainsSubstring("no commit is named no-such-branch"));
    CHECK(run(repo, {"--commit", "HEAD^{tree}"}).status == 2);
}

TEST_CASE("a check of a commit fails when the output root is outside the "
    "checkout", "[app][commit]") {
    scoped_git_repo repo;
    commit_all(repo);
    auto o = at_head(repo);
    o.output_root = repo.output_root();
    CHECK_THROWS_WITH(app::check(o), ContainsSubstring("outside the checkout"));
}

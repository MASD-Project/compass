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
#include "compass.bench/ops/codecs_stage.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.bench/io/report.hpp"
#include "scoped_git_repo.hpp"

namespace {

using namespace compass::bench;
using Catch::Matchers::ContainsSubstring;
using tests::scoped_git_repo;

/**
 * @brief A task contract, a complete task, a task with no Result section
 * and a document with no type.
 */
void fill(const scoped_git_repo& repo) {
    repo.write("doc/meta/document_type_task.org",
        "#+title: Document type: task\n#+type: meta\n\n* Contract\n\n"
        "- Folder :: =task.org=\n- Level :: =s1=\n"
        "- TODO vocabulary :: =BACKLOG | DONE=\n"
        "- Required sections ::\n  - =* Goal= — one paragraph.\n"
        "  - =* Result= — filled at the end.\n");
    repo.write("doc/task_a.org",
        ":PROPERTIES:\n:ID: A-1\n:END:\n#+title: Task: a\n#+type: task\n"
        "* Goal\n* Result\n");
    repo.write("doc/task_b.org",
        "#+title: Task: b\n#+type: task\n* Goal\n");
    repo.write("readme.org", "Plain text.\n");
    repo.git("add -A");
}

types::codecs_result run(const scoped_git_repo& repo,
    const compass::codec::types::configuration& settings = {},
    unsigned repetitions = 2) {
    return ops::run_codecs_stage(repo.checkout(), repo.list(),
        repo.checkout() / "doc" / "meta", repo.packs(), settings, 2,
        repetitions);
}

std::set<std::string> files_of(const std::filesystem::path& root) {
    std::set<std::string> out;
    for (const auto& entry :
        std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() &&
            entry.path().generic_string().find("/.git/") == std::string::npos)
            out.insert(entry.path().lexically_relative(root).generic_string());
    }
    return out;
}

}

TEST_CASE("the codecs stage loads the corpus, runs both codecs and writes "
    "no output to the checkout", "[ops][codecs_stage]") {
    scoped_git_repo repo;
    fill(repo);
    const auto before = files_of(repo.checkout());

    const auto r = run(repo);

    CHECK(r.files == 4);
    CHECK(r.documents == 4);
    CHECK(r.written == 4);
    CHECK(r.loaded == 0);
    CHECK(r.errors == 0);
    CHECK(r.wall_ms.size() == 2);
    CHECK_FALSE(io::failed(r));
    CHECK(files_of(repo.checkout()) == before);

    REQUIRE(r.registry.size() == 5);
    REQUIRE(r.registry.codecs[0].name == "document_index");
    CHECK(r.last.documents[0] == 4);
    REQUIRE(r.last.files[0].size() == 1);
    CHECK(std::string(r.last.files[0].contents[{0}]) ==
        "id\ttitle\ttype\tpath\n"
        "\tDocument type: task\tmeta\tdoc/meta/document_type_task.org\n"
        "A-1\tTask: a\ttask\tdoc/task_a.org\n"
        "\tTask: b\ttask\tdoc/task_b.org\n"
        "\t\t\treadme.org\n");

    REQUIRE(r.registry.codecs[1].name == "required_sections");
    CHECK(r.last.documents[1] == 2);
    REQUIRE(r.last.diagnostics[1].size() == 1);
    CHECK(r.findings[1].documents == 1);
    REQUIRE(r.findings[1].first.size() == 1);
    CHECK(r.findings[1].first[0] ==
        "doc/task_b.org:2: error: required_sections/missing_section: "
        "task document has no \"Result\" section. "
        "Add the heading \"* Result\"; the task contract requires it.");
}

TEST_CASE("writing the outputs of the codecs stage writes the index once "
    "and leaves it alone while nothing changes", "[ops][codecs_stage]") {
    scoped_git_repo repo;
    fill(repo);
    const auto root = repo.checkout() / "generated";
    const auto write = [&] {
        return ops::run_codecs_stage(repo.checkout(), repo.list(),
            repo.checkout() / "doc" / "meta", repo.packs(), {}, 2, 1, root);
    };

    const auto first = write();
    CHECK(first.wrote_outputs);
    CHECK(first.sink.written == 1);
    CHECK(first.outputs_written);
    REQUIRE(files_of(root).size() == 1);
    const auto index = root / *files_of(root).begin();
    const auto written_at = std::filesystem::last_write_time(index);

    const auto second = write();
    CHECK(second.sink.written + second.sink.restored == 0);
    CHECK(second.sink.unchanged == 1);
    CHECK((std::filesystem::last_write_time(index) == written_at));
    std::ostringstream os;
    io::write_codecs_report(os, second, {}, repo.packs());
    CHECK_THAT(os.str(), ContainsSubstring("outputs: 0 written, 1 unchanged"));
    CHECK_FALSE(io::failed(second));
}

TEST_CASE("a second run of the codecs stage loads every object from the "
    "store", "[ops][codecs_stage]") {
    scoped_git_repo repo;
    fill(repo);
    (void)run(repo);

    const auto r = run(repo);

    CHECK(r.loaded == 4);
    CHECK(r.written == 0);
    CHECK(r.last.diagnostics[1].size() == 1);
}

TEST_CASE("a codec the configuration disables is reported as disabled and "
    "emits nothing", "[ops][codecs_stage]") {
    scoped_git_repo repo;
    fill(repo);
    compass::codec::types::configuration settings;
    settings.set("required_sections", "enabled", "false");

    const auto r = run(repo, settings);

    CHECK(r.last.diagnostics[1].size() == 0);
    CHECK(r.last.files[0].size() == 1);
    std::ostringstream os;
    io::write_codecs_report(os, r, {}, repo.packs());
    CHECK_THAT(os.str(), ContainsSubstring("disabled"));
    CHECK_FALSE(io::failed(r));
}

TEST_CASE("the codecs report names each output and counts the findings",
    "[ops][codecs_stage]") {
    scoped_git_repo repo;
    fill(repo);
    const auto r = run(repo);

    std::ostringstream os;
    io::write_codecs_report(os, r, {}, repo.packs());

    CHECK_THAT(os.str(),
        ContainsSubstring("doc/document_index.tsv  ") &&
        ContainsSubstring("diagnostics of required_sections: 1 in 1 "
                          "document(s)") &&
        ContainsSubstring("       1  missing_section") &&
        ContainsSubstring(
            "       1  task document has no \"Result\" section"));
}

TEST_CASE("a setting for a codec nobody registered fails the stage",
    "[ops][codecs_stage]") {
    scoped_git_repo repo;
    fill(repo);
    compass::codec::types::configuration settings;
    settings.set("html", "enabled", "true");

    const auto r = run(repo, settings, 1);

    CHECK(io::failed(r));
    std::ostringstream os;
    io::write_codecs_report(os, r, {}, repo.packs());
    CHECK_THAT(os.str(), ContainsSubstring("registry problem: ") &&
        ContainsSubstring("FAILED: the codec registry has 1 problem(s)."));
}

TEST_CASE("a second run of the codecs stage with nothing changed calls no "
    "unit and leaves the dependency record alone", "[ops][codecs_stage]") {
    scoped_git_repo repo;
    fill(repo);
    const auto first = run(repo);
    CHECK(first.last.ran == std::vector<std::uint64_t>{1, 2, 0, 0, 4});
    CHECK(first.record_written);
    CHECK(first.record_problem.empty());

    const auto second = run(repo);

    CHECK(second.record_units == 7);
    CHECK(second.last.ran == std::vector<std::uint64_t>{0, 0, 0, 0, 0});
    CHECK(second.last.skipped == std::vector<std::uint64_t>{1, 2, 0, 0, 4});
    CHECK_FALSE(second.record_written);
    CHECK(second.last.diagnostics[1].size() == 1);
    CHECK(std::string(second.last.files[0].contents[{0}]) ==
        std::string(first.last.files[0].contents[{0}]));
    std::ostringstream os;
    io::write_codecs_report(os, second, {}, repo.packs());
    CHECK_THAT(os.str(), ContainsSubstring("7 units") &&
        ContainsSubstring("unchanged"));
}

TEST_CASE("editing one task calls its unit and the corpus unit only",
    "[ops][codecs_stage]") {
    scoped_git_repo repo;
    fill(repo);
    (void)run(repo);
    repo.write("doc/task_b.org",
        "#+title: Task: b\n#+type: task\n* Goal\n* Result\n");

    const auto r = run(repo);

    CHECK(r.last.ran == std::vector<std::uint64_t>{1, 1, 0, 0, 1});
    CHECK(r.last.skipped == std::vector<std::uint64_t>{0, 1, 0, 0, 3});
    CHECK(r.last.diagnostics[1].size() == 0);
    CHECK(r.record_written);
}

namespace {

/**
 * @brief Every entry under @p dir: the bytes and modification time of
 * each file, and each directory, so a write, a touch or a new directory
 * all show.
 */
std::map<std::string, std::pair<std::string, std::int64_t>> snapshot(
    const std::filesystem::path& dir) {
    std::map<std::string, std::pair<std::string, std::int64_t>> out;
    for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
        const auto name = e.path().generic_string();
        if (!e.is_regular_file()) {
            out[name] = {"directory", 0};
            continue;
        }
        std::ifstream is(e.path(), std::ios::binary);
        out[name] = {std::string(std::istreambuf_iterator<char>(is), {}),
            e.last_write_time().time_since_epoch().count()};
    }
    return out;
}

types::codecs_diff diff(const scoped_git_repo& repo,
    const std::filesystem::path& root) {
    return ops::diff_codecs_stage(repo.checkout(), repo.list(),
        repo.checkout() / "doc" / "meta", repo.packs(), {}, 2, 1, root);
}

types::codecs_result write(const scoped_git_repo& repo,
    const std::filesystem::path& root) {
    return ops::run_codecs_stage(repo.checkout(), repo.list(),
        repo.checkout() / "doc" / "meta", repo.packs(), {}, 2, 1, root);
}

}

TEST_CASE("diffing a checkout never run before writes nothing and shows "
    "every output as new", "[ops][codecs_stage]") {
    scoped_git_repo repo;
    fill(repo);
    const auto all = repo.checkout().parent_path();
    const auto root = repo.checkout() / "generated";
    const auto before = snapshot(all);

    const auto d = diff(repo, root);

    CHECK(snapshot(all) == before);
    CHECK_FALSE(std::filesystem::exists(repo.packs()));
    CHECK(d.run.parsed == 4);
    CHECK(d.run.written == 0);
    CHECK(d.planned.written == 1);
    CHECK(d.diff.starts_with("--- /dev/null\n"
                             "+++ b/doc/document_index.tsv\n"
                             "@@ -0,0 +1,5 @@\n"));
    CHECK(io::diff_status(d) == 1);
}

TEST_CASE("diffing after a write finds nothing, then the edit, and leaves "
    "the outputs and every record alone", "[ops][codecs_stage]") {
    scoped_git_repo repo;
    fill(repo);
    const auto all = repo.checkout().parent_path();
    const auto root = repo.checkout() / "generated";
    // An edit git has not staged gives the worktree index an entry.
    repo.write("readme.org", "Plain text, edited.\n");
    REQUIRE(write(repo, root).wrote_outputs);
    const auto records = repo.checkout() / ".git" / "compass";
    REQUIRE(std::filesystem::exists(records / "index"));
    REQUIRE(std::filesystem::exists(records / "outputs"));
    REQUIRE(std::filesystem::exists(records / "deps"));

    auto before = snapshot(all);
    const auto clean = diff(repo, root);
    CHECK(snapshot(all) == before);
    CHECK(clean.diff.empty());
    CHECK(clean.planned.unchanged == 1);
    CHECK(io::diff_status(clean) == 0);

    repo.write("doc/task_b.org", "#+title: Task: b2\n#+type: task\n* Goal\n");
    before = snapshot(all);
    const auto edited = diff(repo, root);
    CHECK(snapshot(all) == before);
    CHECK(edited.diff ==
        "--- a/doc/document_index.tsv\n"
        "+++ b/doc/document_index.tsv\n"
        "@@ -1,5 +1,5 @@\n"
        " id\ttitle\ttype\tpath\n"
        " \tDocument type: task\tmeta\tdoc/meta/document_type_task.org\n"
        " A-1\tTask: a\ttask\tdoc/task_a.org\n"
        "-\tTask: b\ttask\tdoc/task_b.org\n"
        "+\tTask: b2\ttask\tdoc/task_b.org\n"
        " \t\t\treadme.org\n");
    CHECK(io::diff_status(edited) == 1);
    std::ostringstream os;
    io::write_codecs_diff_summary(os, edited, {});
    CHECK_THAT(os.str(), ContainsSubstring(
        "would write 1, restore 0, remove 0, keep 0; 0 unchanged"));

    const auto written = write(repo, root);
    CHECK(written.sink.written == edited.planned.written);
    CHECK(written.sink.unchanged == edited.planned.unchanged);
    CHECK(io::diff_status(diff(repo, root)) == 0);
}

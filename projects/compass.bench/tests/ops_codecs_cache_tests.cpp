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

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.bench/io/report.hpp"
#include "compass.bench/ops/gc_stage.hpp"
#include "compass.bench/ops/listing.hpp"
#include "compass.codec/io/dependency_file.hpp"
#include "compass.codec/ops/dependencies.hpp"
#include "compass.store/io/action_file.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "scoped_git_repo.hpp"

namespace {

using namespace compass::bench;
using compass::store::types::action_kind;
using Catch::Matchers::ContainsSubstring;
using tests::scoped_git_repo;
using counts = std::vector<std::uint64_t>;

/**
 * @brief A repository with a task contract, two tasks and a plain
 * document committed once, and a linked worktree on that commit.
 */
class two_worktrees final {
public:
    two_worktrees() {
        repo_.write("doc/meta/document_type_task.org",
            "#+title: Document type: task\n#+type: meta\n\n* Contract\n\n"
            "- Folder :: =task.org=\n- Level :: =s1=\n"
            "- TODO vocabulary :: =BACKLOG | DONE=\n"
            "- Required sections ::\n  - =* Goal= — one paragraph.\n"
            "  - =* Result= — filled at the end.\n");
        repo_.write("doc/task_a.org",
            ":PROPERTIES:\n:ID: A-1\n:END:\n#+title: Task: a\n"
            "#+type: task\n* Goal\n* Result\n");
        repo_.write("doc/task_b.org",
            "#+title: Task: b\n#+type: task\n* Goal\n");
        repo_.write("readme.org", "Plain text.\n");
        repo_.git("add -A");
        repo_.git("-c user.name=t -c user.email=t@example.org "
                  "-c commit.gpgsign=false commit -q -m base");
        repo_.git("worktree add -q '" + second().string() + "'");
    }

    [[nodiscard]] const scoped_git_repo& repo() const { return repo_; }
    [[nodiscard]] std::filesystem::path first() const {
        return repo_.checkout();
    }
    [[nodiscard]] std::filesystem::path second() const {
        return repo_.checkout().parent_path() / "second";
    }

    [[nodiscard]] std::filesystem::path common() const {
        return compass::store::io::git_common_dir(first());
    }

    /**
     * @brief Runs the codecs once in @p checkout, writing the outputs under
     * @p out.
     */
    types::codecs_result run(const std::filesystem::path& checkout,
        const std::filesystem::path& out,
        const compass::codec::types::configuration& settings = {}) const {
        const auto table = ops::build_file_table(checkout,
            ops::parse_listing(ops::git_ls_files(checkout)));
        return ops::run_codecs_stage(checkout, table,
            checkout / "doc" / "meta", repo_.objects(), settings, 2, 1, out);
    }

    [[nodiscard]] std::filesystem::path out(const std::string& name) const {
        return repo_.checkout().parent_path() / name;
    }

    [[nodiscard]] std::vector<std::filesystem::path> action_files(
        action_kind kind) const {
        std::vector<std::filesystem::path> files;
        const auto dir = compass::store::io::actions_dir(common(), kind);
        for (const auto& e :
            std::filesystem::recursive_directory_iterator(dir)) {
            if (e.is_regular_file())
                files.push_back(e.path());
        }
        return files;
    }

private:
    scoped_git_repo repo_;
};

std::map<std::string, std::string> contents(const std::filesystem::path& root) {
    std::map<std::string, std::string> out;
    for (const auto& e : std::filesystem::recursive_directory_iterator(root)) {
        if (!e.is_regular_file())
            continue;
        std::ifstream is(e.path(), std::ios::binary);
        out[e.path().lexically_relative(root).string()] =
            std::string(std::istreambuf_iterator<char>(is), {});
    }
    return out;
}

std::string record_of(const types::codecs_result& r) {
    return compass::codec::ops::encode_dependency_record(r.last.record);
}

void age_all(const std::filesystem::path& dir) {
    for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
        if (e.is_regular_file())
            std::filesystem::last_write_time(e.path(),
                std::filesystem::file_time_type::clock::now() -
                    std::chrono::hours(2));
    }
}

}

TEST_CASE("a second worktree on the same commit runs no unit and writes "
    "the first's outputs byte for byte", "[ops][codecs_cache]") {
    two_worktrees w;
    const auto first = w.run(w.first(), w.out("out1"));
    CHECK(first.last.ran == counts{1, 2});

    const auto second = w.run(w.second(), w.out("out2"));

    CHECK(second.last.ran == counts{0, 0});
    CHECK(second.last.skipped == counts{0, 0});
    CHECK(second.last.cached == counts{1, 2});
    CHECK(second.actions.results == 0);
    CHECK(contents(w.out("out2")) == contents(w.out("out1")));
    CHECK(record_of(second) == record_of(first));
    std::ostringstream os;
    io::write_codecs_report(os, second, {}, w.repo().objects());
    CHECK_THAT(os.str(), ContainsSubstring("cached") &&
        ContainsSubstring("stored 0 results"));
}

TEST_CASE("an edit in one worktree runs that document's unit and the "
    "corpus unit only", "[ops][codecs_cache]") {
    two_worktrees w;
    (void)w.run(w.first(), w.out("out1"));
    (void)w.run(w.second(), w.out("out2"));
    w.repo().write("doc/task_b.org",
        "#+title: Task: b\n#+type: task\n* Goal\n* Result\n");

    const auto edited = w.run(w.first(), w.out("out1"));
    const auto other = w.run(w.second(), w.out("out2"));

    CHECK(edited.last.ran == counts{1, 1});
    CHECK(edited.last.skipped == counts{0, 1});
    CHECK(edited.last.diagnostics[1].size() == 0);
    CHECK(other.last.ran == counts{0, 0});
    CHECK(other.last.skipped == counts{1, 2});
    CHECK(other.last.diagnostics[1].size() == 1);
}

TEST_CASE("a configuration the first worktree did not run with misses the "
    "cache", "[ops][codecs_cache]") {
    two_worktrees w;
    (void)w.run(w.first(), w.out("out1"));
    compass::codec::types::configuration settings;
    settings.set("document_index", "path", "other.tsv");

    const auto second = w.run(w.second(), w.out("out2"), settings);

    CHECK(second.last.ran == counts{1, 0});
    CHECK(second.last.cached == counts{0, 2});
    CHECK(contents(w.out("out2")).contains("other.tsv"));
}

TEST_CASE("damaged action objects are ignored and their units run",
    "[ops][codecs_cache]") {
    two_worktrees w;
    const auto first = w.run(w.first(), w.out("out1"));
    for (const auto kind : {action_kind::manifest, action_kind::result}) {
        for (const auto& file : w.action_files(kind)) {
            std::fstream f(file,
                std::ios::binary | std::ios::in | std::ios::out);
            f.seekp(9);
            f.put('!');
        }
    }

    const auto second = w.run(w.second(), w.out("out2"));

    CHECK(second.last.ran == counts{1, 2});
    CHECK(second.last.cached == counts{0, 0});
    CHECK(contents(w.out("out2")) == contents(w.out("out1")));
    CHECK(record_of(second) == record_of(first));
}

TEST_CASE("collection keeps the action objects any worktree's record names "
    "and removes old ones none names", "[ops][codecs_cache]") {
    two_worktrees w;
    (void)w.run(w.first(), w.out("out1"));
    std::ofstream(w.second() / "doc" / "task_b.org", std::ios::binary)
        << "#+title: Task: b\n#+type: task\n* Goal\n* Result\n";
    const auto second = w.run(w.second(), w.out("out2"));
    REQUIRE(second.last.ran == counts{1, 1});
    const auto orphan = compass::store::ops::blob_id_of("orphan");
    compass::store::io::write_action(compass::store::io::actions_dir(
        w.common(), action_kind::result), orphan, "unnamed");
    age_all(w.common() / "compass");

    const auto r = ops::run_gc_stage(w.first(), w.repo().objects(),
        {0, std::chrono::hours(1), false});

    CHECK(r.worktrees.size() == 2);
    for (const auto& checkout : {w.first(), w.second()}) {
        const auto loaded = compass::codec::io::read_dependency_file(
            compass::codec::io::dependency_file_path(
                compass::store::io::git_dir(checkout)));
        REQUIRE(loaded.record.size() == 3);
        for (std::uint32_t u = 0; u < loaded.record.size(); ++u) {
            CHECK(compass::store::io::read_action(
                compass::store::io::actions_dir(w.common(),
                    action_kind::manifest), loaded.record.manifests[{u}]));
            CHECK(compass::store::io::read_action(
                compass::store::io::actions_dir(w.common(),
                    action_kind::result), loaded.record.results[{u}]));
        }
    }
    CHECK_FALSE(compass::store::io::read_action(
        compass::store::io::actions_dir(w.common(), action_kind::result),
        orphan));
    CHECK(r.removed.objects == 1);
    std::ostringstream os;
    io::write_gc_report(os, r, {}, w.repo().objects());
    CHECK_THAT(os.str(), ContainsSubstring("action objects"));
}

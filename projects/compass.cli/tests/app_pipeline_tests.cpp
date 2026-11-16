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

#include <cstddef>
#include <filesystem>
#include <catch2/catch_test_macros.hpp>
#include "compass.store/io/location.hpp"
#include "scoped_git_repo.hpp"

namespace {

using namespace compass::cli;
using tests::scoped_git_repo;

types::options options_of(const scoped_git_repo& repo) {
    types::options o;
    o.checkout = repo.checkout();
    o.meta = repo.checkout() / "doc" / "meta";
    o.output_root = repo.output_root();
    o.threads = 2;
    return o;
}

std::size_t files_under(const std::filesystem::path& dir) {
    std::size_t n = 0;
    for (const auto& entry :
        std::filesystem::recursive_directory_iterator(dir))
        n += entry.is_regular_file() ? 1 : 0;
    return n;
}

}

TEST_CASE("check loads every org file, stores what it parsed and writes no "
    "output", "[app][pipeline]") {
    scoped_git_repo repo;

    const auto first = app::check(options_of(repo));
    CHECK(first.run.files == 4);
    CHECK(first.run.documents == 4);
    CHECK(first.run.parsed == 4);
    CHECK_FALSE(first.run.failed());
    REQUIRE(first.findings.size() == 1);
    CHECK(first.findings[0].path == "doc/task_b.org");
    CHECK(first.findings[0].line == 2);
    CHECK(first.findings[0].column == 9);
    CHECK_FALSE(std::filesystem::exists(repo.output_root()));

    const auto second = app::check(options_of(repo));
    CHECK(second.run.loaded == 4);
    CHECK(second.run.parsed == 0);
}

TEST_CASE("an untracked org file is checked and a deleted one is not",
    "[app][pipeline]") {
    scoped_git_repo repo;
    repo.write("doc/task_c.org", "#+title: Task: c\n#+type: task\n");
    std::filesystem::remove(repo.checkout() / "doc" / "task_a.org");

    const auto r = app::check(options_of(repo));

    CHECK(r.run.files == 4);
    CHECK_FALSE(r.run.failed());
    REQUIRE(r.findings.size() == 3);
    CHECK(r.findings[0].path == "doc/task_b.org");
    CHECK(r.findings[1].path == "doc/task_c.org");
    CHECK(r.findings[2].path == "doc/task_c.org");
}

TEST_CASE("a setting nobody declared fails the run and writes no output",
    "[app][pipeline]") {
    scoped_git_repo repo;
    auto o = options_of(repo);
    o.settings.set("html", "enabled", "true");

    const auto r = app::generate(o);

    CHECK(r.run.failed());
    CHECK(r.run.problems.size() == 1);
    CHECK_FALSE(std::filesystem::exists(repo.output_root()));
}

TEST_CASE("gc with --dry-run removes nothing that a real gc removes",
    "[app][pipeline]") {
    scoped_git_repo repo;
    const auto store =
        compass::store::io::git_common_dir(repo.checkout()) / "compass";
    (void)app::check(options_of(repo));
    repo.write("readme.org", "Edited once.\n");
    (void)app::check(options_of(repo));
    repo.write("readme.org", "Edited twice.\n");
    (void)app::check(options_of(repo));
    auto o = options_of(repo);
    o.collection.grace = std::chrono::seconds(0);
    o.collection.dry_run = true;
    const auto before = files_under(store);

    const auto dry = app::gc(o);

    CHECK(dry.collection.plan.victims.objects >= 1);
    CHECK(dry.collection.removed.objects == 0);
    CHECK(files_under(store) == before);

    o.collection.dry_run = false;
    const auto real = app::gc(o);
    CHECK(real.collection.removed.objects ==
        dry.collection.plan.victims.objects);
    CHECK(files_under(store) ==
        before - real.collection.removed.objects);
}

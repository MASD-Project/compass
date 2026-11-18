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
#include "compass.store/io/location.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.platform/process/run_program.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"
#include "files.hpp"

namespace {

using namespace compass::store;
using compass::testing::filesystem::scoped_directory;

void git(const std::filesystem::path& directory, const std::string& command) {
    const auto line = "git -C '" + directory.generic_string() +
        "' -c user.name=t -c user.email=t@example.org " + command +
        " >/dev/null 2>&1";
    REQUIRE(compass::platform::process::run_program(
        {{"sh", "-c", line}, {}, {}}).exit_code == 0);
}

}

TEST_CASE("the common directory of a main checkout is its .git",
    "[io][location]") {
    const scoped_directory d("compass.store.tests");
    git(d.path(), "init -q");

    CHECK(io::git_common_dir(d.path()) == d.path() / ".git");
}

TEST_CASE("a linked worktree shares the common directory of its repository",
    "[io][location]") {
    const scoped_directory d("compass.store.tests");
    const auto main = d.path() / "main";
    const auto linked = d.path() / "linked";
    std::filesystem::create_directories(main);
    git(main, "init -q");
    git(main, "commit -q --allow-empty -m base");
    git(main, "worktree add -q '" + linked.generic_string() + "'");

    CHECK(io::git_common_dir(linked) == main / ".git");
    CHECK(io::packs_dir(io::git_common_dir(linked)) ==
        io::packs_dir(io::git_common_dir(main)));
}

TEST_CASE("a relative gitdir and commondir resolve against their files",
    "[io][location]") {
    const scoped_directory d("compass.store.tests");
    tests::write(d.path() / "tree/.git", "gitdir: ../repo/worktrees/tree\n");
    tests::write(d.path() / "repo/worktrees/tree/commondir", "../..\n");

    CHECK(io::git_common_dir(d.path() / "tree") == d.path() / "repo");
}

TEST_CASE("a gitdir with no commondir is its own common directory",
    "[io][location]") {
    const scoped_directory d("compass.store.tests");
    tests::write(d.path() / "tree/.git", "gitdir: ../modules/tree\n");
    std::filesystem::create_directories(d.path() / "modules" / "tree");

    CHECK(io::git_common_dir(d.path() / "tree") ==
        d.path() / "modules" / "tree");
}

TEST_CASE("a directory that is not a checkout has no common directory",
    "[io][location]") {
    const scoped_directory d("compass.store.tests");
    tests::write(d.path() / "broken/.git", "not a gitdir line\n");

    CHECK_THROWS_AS(io::git_common_dir(d.path()), std::runtime_error);
    CHECK_THROWS_AS(io::git_common_dir(d.path() / "broken"),
        std::runtime_error);
}

TEST_CASE("the packs and the first layout of the store live under the "
    "common directory", "[io][location]") {
    CHECK(io::packs_dir("/repo/.git") == "/repo/.git/compass/packs");
    CHECK(io::legacy_store_dirs("/repo/.git") ==
        std::vector<std::filesystem::path>{"/repo/.git/compass/objects",
            "/repo/.git/compass/manifests", "/repo/.git/compass/results"});
}

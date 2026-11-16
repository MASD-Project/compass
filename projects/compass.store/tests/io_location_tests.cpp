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
#include "compass.store/ops/blob_hash.hpp"
#include "scoped_directory.hpp"

namespace {

using namespace compass::store;
using tests::scoped_directory;

void git(const std::filesystem::path& directory, const std::string& command) {
    const auto line = "git -C '" + directory.string() +
        "' -c user.name=t -c user.email=t@example.org " + command +
        " >/dev/null 2>&1";
    REQUIRE(std::system(line.c_str()) == 0);
}

}

TEST_CASE("the common directory of a main checkout is its .git",
    "[io][location]") {
    scoped_directory d;
    git(d.root(), "init -q");

    CHECK(io::git_common_dir(d.root()) == d.root() / ".git");
}

TEST_CASE("a linked worktree shares the common directory of its repository",
    "[io][location]") {
    scoped_directory d;
    const auto main = d.root() / "main";
    const auto linked = d.root() / "linked";
    std::filesystem::create_directories(main);
    git(main, "init -q");
    git(main, "commit -q --allow-empty -m base");
    git(main, "worktree add -q '" + linked.string() + "'");

    CHECK(io::git_common_dir(linked) == main / ".git");
    CHECK(io::objects_dir(io::git_common_dir(linked)) ==
        io::objects_dir(io::git_common_dir(main)));
}

TEST_CASE("a relative gitdir and commondir resolve against their files",
    "[io][location]") {
    scoped_directory d;
    d.write("tree/.git", "gitdir: ../repo/worktrees/tree\n");
    d.write("repo/worktrees/tree/commondir", "../..\n");

    CHECK(io::git_common_dir(d.root() / "tree") == d.root() / "repo");
}

TEST_CASE("a gitdir with no commondir is its own common directory",
    "[io][location]") {
    scoped_directory d;
    d.write("tree/.git", "gitdir: ../modules/tree\n");
    std::filesystem::create_directories(d.root() / "modules" / "tree");

    CHECK(io::git_common_dir(d.root() / "tree") ==
        d.root() / "modules" / "tree");
}

TEST_CASE("a directory that is not a checkout has no common directory",
    "[io][location]") {
    scoped_directory d;
    d.write("broken/.git", "not a gitdir line\n");

    CHECK_THROWS_AS(io::git_common_dir(d.root()), std::runtime_error);
    CHECK_THROWS_AS(io::git_common_dir(d.root() / "broken"),
        std::runtime_error);
}

TEST_CASE("an object lives under the first two digits of its ID",
    "[io][location]") {
    const auto id = ops::blob_id_of("hello\n");

    CHECK(io::object_path("/repo/.git/compass/objects", id) ==
        "/repo/.git/compass/objects/ce/"
        "013625030ba8dba906f756967f9e9ca394464a");
    CHECK(io::objects_dir("/repo/.git") == "/repo/.git/compass/objects");
}

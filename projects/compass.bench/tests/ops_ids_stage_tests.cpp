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
#include "compass.bench/ops/ids_stage.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.bench/io/report.hpp"
#include "compass.bench/ops/listing.hpp"
#include "compass.platform/process/run_program.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"

namespace {

using namespace compass::bench;

constexpr auto all = std::numeric_limits<std::uint64_t>::max();

class scoped_git_repo final {
    compass::testing::filesystem::scoped_directory dir_{
        "compass.bench.ids.tests"};

public:
    scoped_git_repo() {
        git("init -q");
    }

    /**
     * @brief Writes a file an hour in the past, so the index git writes
     * next is not racy for it.
     */
    void write(const std::string& relative, std::string_view content) {
        const auto path = dir_.path() / relative;
        std::filesystem::create_directories(path.parent_path());
        {
            std::ofstream os(path, std::ios::binary);
            os.write(content.data(),
                static_cast<std::streamsize>(content.size()));
        }
        std::filesystem::last_write_time(path,
            std::filesystem::file_time_type::clock::now() -
                std::chrono::hours(1));
    }

    void git(const std::string& command) const {
        const auto line = "git -C '" + dir_.path().generic_string() + "' " +
            command + " >/dev/null 2>&1";
        REQUIRE(compass::platform::process::run_program(
            {{"sh", "-c", line}, {}, {}}).exit_code == 0);
    }

    types::file_table list() const {
        const auto raw = ops::git_ls_files(dir_.path());
        return ops::build_file_table(dir_.path(), ops::parse_listing(raw));
    }

    const std::filesystem::path& root() const { return dir_.path(); }
};

}

TEST_CASE("the ids stage counts IDs by source and every one matches git",
    "[ops][ids_stage]") {
    scoped_git_repo repo;
    repo.write(".gitattributes", "*.sh text eol=lf\n");
    repo.write("a.org", "* A\n");
    repo.write("dir/b.org", "* B\n");
    repo.write("run.sh", "echo\n");
    repo.write("changed.txt", "before\n");
    repo.git("add -A");
    repo.write("changed.txt", "after, longer\n");

    const auto table = repo.list();
    const auto r = ops::run_ids_stage(repo.root(), table, 2, 3, all);

    CHECK(r.files == 5);
    CHECK(r.from_index == 3);
    CHECK(r.hashed == 2);
    CHECK(r.errors == 0);
    CHECK(r.index_entries == 5);
    CHECK(r.read_ms.size() == 3);
    CHECK(r.stat_ms.size() == 3);
    CHECK(r.hash_ms.size() == 3);
    CHECK(r.verified == 5);
    CHECK(r.mismatches == 0);
    CHECK_FALSE(io::failed(r));

    std::ostringstream os;
    io::write_ids_report(os, r, {});
    CHECK_THAT(os.str(), Catch::Matchers::ContainsSubstring(
        "verified against git hash-object: 5  mismatches: 0"));
}

TEST_CASE("the ids stage verifies an even sample", "[ops][ids_stage]") {
    scoped_git_repo repo;
    for (int i = 0; i < 10; ++i)
        repo.write("f" + std::to_string(i) + ".txt", std::to_string(i));
    repo.git("add -A");

    const auto r = ops::run_ids_stage(repo.root(), repo.list(), 1, 1, 4);
    CHECK(r.verified == 4);
    CHECK(r.mismatches == 0);
}

TEST_CASE("the ids stage fails when a tracked file cannot be hashed",
    "[ops][ids_stage]") {
    scoped_git_repo repo;
    repo.write("a.txt", "a");
    repo.write("gone.txt", "g");
    repo.git("add -A");
    const auto table = repo.list();
    std::filesystem::remove(repo.root() / "gone.txt");

    const auto r = ops::run_ids_stage(repo.root(), table, 1, 1, 0);
    CHECK(r.errors == 1);
    CHECK(io::failed(r));
}

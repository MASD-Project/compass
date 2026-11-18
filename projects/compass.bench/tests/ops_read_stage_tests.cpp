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
#include "compass.bench/ops/read_stage.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.bench/ops/listing.hpp"
#include "compass.bench/ops/partition.hpp"
#include "compass.platform/process/environment.hpp"
#include "compass.platform/process/run_program.hpp"

namespace {

using namespace compass::bench;

class scoped_git_repo final {
public:
    scoped_git_repo() {
        static int counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("compass.bench.tests." +
                std::to_string(
                    compass::platform::process::current_process_id()) +
                "." + std::to_string(counter++));
        std::filesystem::create_directories(root_);
    }
    ~scoped_git_repo() { std::filesystem::remove_all(root_); }
    scoped_git_repo(const scoped_git_repo&) = delete;
    scoped_git_repo& operator=(const scoped_git_repo&) = delete;

    void write(const std::string& relative, std::string_view content) {
        const auto path = root_ / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream os(path, std::ios::binary);
        os.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    void track_all() {
        const auto command = "git -C '" + root_.string() +
            "' init -q && git -C '" + root_.string() + "' add -A";
        REQUIRE(compass::platform::process::run_program(
            {{"sh", "-c", command}, {}, {}}).exit_code == 0);
    }

    const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

std::uint64_t byte_sum(std::string_view s) {
    std::uint64_t r = 0;
    for (const unsigned char c : s)
        r += c;
    return r;
}

types::file_table list(const std::filesystem::path& root) {
    const auto raw = ops::git_ls_files(root);
    return ops::build_file_table(root, ops::parse_listing(raw));
}

}

TEST_CASE("read stage reads every tracked regular file and skips symlinks",
    "[ops][read_stage]") {
    const std::string a = "hello\n";
    const std::string b(10'000, '\xfe');
    const std::string c = "x";
    scoped_git_repo repo;
    repo.write("a.cpp", a);
    repo.write("dir/with space.hpp", b);
    repo.write("empty.txt", "");
    repo.write("sub/c", c);
    std::filesystem::create_symlink("a.cpp", repo.root() / "link.cpp");
    repo.track_all();

    const auto table = list(repo.root());
    REQUIRE(table.size() == 4);
    CHECK(std::ranges::find(table.paths, "link.cpp") == table.paths.end());

    const auto ranges = ops::balance_ranges(table.sizes, 3);
    const auto result = ops::run_read_stage(repo.root(), table, ranges, 2);

    CHECK(result.files == 4);
    CHECK(result.bytes == a.size() + b.size() + c.size());
    CHECK(result.errors == 0);
    CHECK(result.checksum == byte_sum(a) + byte_sum(b) + byte_sum(c));
    CHECK(result.wall_ms.size() == 2);
}

TEST_CASE("read stage counts a deleted but listed file as an error",
    "[ops][read_stage]") {
    const std::string kept = "kept";
    scoped_git_repo repo;
    repo.write("kept.cpp", kept);
    repo.write("gone.cpp", "gone");
    repo.track_all();
    std::filesystem::remove(repo.root() / "gone.cpp");

    const auto table = list(repo.root());
    REQUIRE(table.size() == 2);

    const auto ranges = ops::balance_ranges(table.sizes, 2);
    const auto result = ops::run_read_stage(repo.root(), table, ranges, 1);

    CHECK(result.files == 1);
    CHECK(result.bytes == kept.size());
    CHECK(result.errors == 1);
    CHECK(result.checksum == byte_sum(kept));
    CHECK_THAT(result.first_error,
        Catch::Matchers::ContainsSubstring("gone.cpp"));
}

TEST_CASE("git_ls_files of a directory outside any repository throws",
    "[ops][listing]") {
    scoped_git_repo dir;

    CHECK_THROWS(ops::git_ls_files(dir.root()));
}

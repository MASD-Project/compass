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
#include "compass.bench/ops/org_stage.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.bench/ops/listing.hpp"
#include "compass.bench/ops/org_dump.hpp"

namespace {

using namespace compass::bench;

class scoped_git_repo final {
public:
    scoped_git_repo() {
        static int counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("compass.bench.org.tests." + std::to_string(::getpid()) + "." +
                std::to_string(counter++));
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
        REQUIRE(std::system(command.c_str()) == 0);
    }

    std::string read(const std::string& relative) const {
        std::ifstream is(root_ / relative, std::ios::binary);
        std::ostringstream os;
        os << is.rdbuf();
        return os.str();
    }

    const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

types::file_table list(const std::filesystem::path& root) {
    const auto raw = ops::git_ls_files(root);
    return ops::build_file_table(root, ops::parse_listing(raw));
}

constexpr std::string_view heading_dump =
    "org-data 0 4 0 4 0\n"
    "  headline 0 4 - - 0 level=1 todo=- todo-type=- priority=- commented=0 "
    "tags=- title=H\n";

}

TEST_CASE("org stage parses every org file and no other", "[ops][org_stage]") {
    const std::string a = "* H\ntext *b* [[id:x]]\n";
    const std::string b = "#+title: t\n\n- item\n";
    scoped_git_repo repo;
    repo.write("a.org", a);
    repo.write("doc/b.org", b);
    repo.write("c.cpp", "int x;\n");
    repo.write("org", "* not an org file\n");
    repo.track_all();

    const auto table = list(repo.root());
    const auto result = ops::run_org_stage(repo.root(), table, 2, 3);

    CHECK(result.stage.name == "org");
    CHECK(result.stage.files == 2);
    CHECK(result.stage.bytes == a.size() + b.size());
    CHECK(result.stage.errors == 0);
    CHECK(result.stage.wall_ms.size() == 3);
    CHECK(result.ids.size() == 2);
    CHECK(result.parse_ms.size() == 2);
    CHECK(result.elements == 4 + 6);
    CHECK(result.objects == 2);
    CHECK(result.headings == 1);
    CHECK(result.links == 1);
}

TEST_CASE("org stage counts a deleted but listed file as an error",
    "[ops][org_stage]") {
    scoped_git_repo repo;
    repo.write("kept.org", "* H\n");
    repo.write("gone.org", "* G\n");
    repo.track_all();
    std::filesystem::remove(repo.root() / "gone.org");

    const auto table = list(repo.root());
    const auto result = ops::run_org_stage(repo.root(), table, 1, 1);

    CHECK(result.stage.files == 1);
    CHECK(result.stage.errors == 1);
    CHECK(result.headings == 1);
    CHECK_THAT(result.stage.first_error,
        Catch::Matchers::ContainsSubstring("gone.org"));
}

TEST_CASE("org dump writes the dump of one file", "[ops][org_dump]") {
    scoped_git_repo repo;
    repo.write("a.org", "* H\n");

    std::ostringstream os;
    ops::dump_org_file(repo.root() / "a.org", os);

    CHECK(os.str() == heading_dump);
}

TEST_CASE("org dump writes one output per line of a list",
    "[ops][org_dump]") {
    scoped_git_repo repo;
    repo.write("a.org", "* H\n");
    repo.write("b.org", "");
    const auto root = repo.root().string();
    repo.write("list", root + "/a.org\t" + root + "/a.dump\n" + root +
        "/b.org\t" + root + "/b.dump\n");

    CHECK(ops::dump_org_list(repo.root() / "list") == 2);
    CHECK(repo.read("a.dump") == heading_dump);
    CHECK(repo.read("b.dump") == "org-data 0 0 - - 0\n");
}

TEST_CASE("org dump rejects a list line without a tab", "[ops][org_dump]") {
    scoped_git_repo repo;
    repo.write("list", "a.org a.dump\n");

    CHECK_THROWS_AS(ops::dump_org_list(repo.root() / "list"),
        std::runtime_error);
}

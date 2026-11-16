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
#include "compass.bench/ops/types_stage.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.bench/io/report.hpp"
#include "compass.bench/ops/listing.hpp"
#include "compass.org/ops/type_lookup.hpp"

namespace {

using namespace compass::bench;
using Catch::Matchers::ContainsSubstring;

class scoped_git_repo final {
public:
    scoped_git_repo() {
        static int counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("compass.bench.types.tests." + std::to_string(::getpid()) + "." +
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

    const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

types::file_table list(const std::filesystem::path& root) {
    const auto raw = ops::git_ls_files(root);
    return ops::build_file_table(root, ops::parse_listing(raw));
}

constexpr std::string_view story_contract =
    "#+title: Document type: story\n"
    "* Contract\n"
    "- Folder :: =versions/=\n"
    "- Level :: =s2=\n"
    "- TODO vocabulary :: =STARTED | DONE=\n"
    "- Required sections ::\n"
    "  - =* Goal= — the outcome.\n";

std::size_t count(const types::types_result& r, types::resolution outcome) {
    return static_cast<std::size_t>(
        std::ranges::count(r.outcomes, outcome));
}

}

TEST_CASE("types stage resolves known types and reports the rest",
    "[ops][types_stage]") {
    scoped_git_repo repo;
    repo.write("doc/meta/document_type_story.org", story_contract);
    repo.write("doc/meta/notes.org", "#+title: Not a contract\n");
    repo.write("a/story.org", "#+title: A\n#+type: story\n");
    repo.write("a/model.org", "#+type: ores.codegen.entity\n");
    repo.write("b/widget.org", "#+title: B\n#+type: widget\n* Heading\n");
    repo.write("c/untyped.org", "#+title: C\n");
    repo.write("c/readme.md", "#+type: story\n");
    repo.track_all();

    const auto table = list(repo.root());
    const auto r = ops::run_types_stage(repo.root(), table,
        repo.root() / "doc/meta", 2, 2);

    CHECK(r.stage.files == 6);
    CHECK(r.stage.errors == 0);
    CHECK(r.stage.wall_ms.size() == 2);
    CHECK(r.registry_ms.size() == 2);
    CHECK(r.meta.paths == std::vector<std::string>{"document_type_story.org"});

    CHECK(count(r, types::resolution::resolved) == 2);
    CHECK(count(r, types::resolution::untyped) == 3);
    REQUIRE(count(r, types::resolution::unresolved) == 1);
    for (std::size_t row = 0; row < r.ids.size(); ++row) {
        const auto& path = table.paths[r.ids[row]];
        if (path == "b/widget.org") {
            CHECK(r.outcomes[row] == types::resolution::unresolved);
            CHECK(r.values[row] == "widget");
        } else if (path == "a/model.org") {
            REQUIRE(r.outcomes[row] == types::resolution::resolved);
            CHECK(r.meta.registry.types.kinds[r.types[row]] ==
                compass::org::types::type_kind::model);
        } else if (path == "a/story.org") {
            REQUIRE(r.outcomes[row] == types::resolution::resolved);
            CHECK(r.types[row] ==
                compass::org::ops::find_type(r.meta.registry, "story"));
        }
    }

    std::ostringstream os;
    io::write_types_report(os, r, {}, table);
    const auto out = os.str();
    CHECK_FALSE(io::failed(r));
    CHECK_THAT(out, ContainsSubstring("unresolved types: 1"));
    CHECK_THAT(out, ContainsSubstring("widget\n            b/widget.org\n"));
    CHECK_THAT(out, ContainsSubstring("documents with no #+type: 3"));
    CHECK_THAT(out, ContainsSubstring("c/untyped.org"));
    CHECK_THAT(out, ContainsSubstring("model  ores.codegen.entity"));
}

TEST_CASE("types stage fails when the meta directory has no contract page",
    "[ops][types_stage]") {
    scoped_git_repo repo;
    repo.write("doc/meta/notes.org", "#+title: Not a contract\n");
    repo.write("a.org", "#+type: story\n");
    repo.track_all();

    const auto table = list(repo.root());
    const auto r = ops::run_types_stage(repo.root(), table,
        repo.root() / "doc/meta", 1, 1);
    CHECK(r.meta.paths.empty());
    CHECK(io::failed(r));
}

TEST_CASE("types stage throws when the meta directory does not exist",
    "[ops][types_stage]") {
    scoped_git_repo repo;
    repo.write("a.org", "#+type: story\n");
    repo.track_all();

    const auto table = list(repo.root());
    CHECK_THROWS(ops::run_types_stage(repo.root(), table,
        repo.root() / "missing", 1, 1));
}

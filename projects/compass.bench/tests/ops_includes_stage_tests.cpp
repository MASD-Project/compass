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
#include "compass.bench/ops/includes_stage.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.bench/io/report.hpp"
#include "compass.bench/ops/listing.hpp"
#include "compass.bench/ops/search_paths.hpp"
#include "compass.platform/process/environment.hpp"
#include "compass.platform/process/run_program.hpp"

namespace {

using namespace compass::bench;

class scoped_git_repo final {
public:
    scoped_git_repo() {
        static int counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("compass.bench.includes." +
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

types::file_table list(const std::filesystem::path& root) {
    const auto raw = ops::git_ls_files(root);
    return ops::build_file_table(root, ops::parse_listing(raw));
}

std::uint32_t id_of(const types::file_table& table, std::string_view path) {
    const auto it = std::ranges::find(table.paths, path);
    REQUIRE(it != table.paths.end());
    return static_cast<std::uint32_t>(it - table.paths.begin());
}

}

TEST_CASE("includes stage scans, resolves and closes a small checkout",
    "[ops][includes_stage]") {
    scoped_git_repo repo;
    repo.write("src/a.cpp", "#include \"a.hpp\"\n#include <missing.hpp>\n");
    repo.write("src/b.cpp", "#include \"a.hpp\"\n");
    repo.write("src/a.hpp", "#include \"c.hpp\"\n");
    repo.write("src/c.hpp", "");
    repo.write("README", "#include \"a.hpp\"\n");
    repo.track_all();

    const auto table = list(repo.root());
    const auto paths = ops::build_search_paths({}, repo.root(), {}, table);
    const auto r = ops::run_includes_stage(repo.root(), table, paths, 3, 2);

    CHECK(r.files == 4);
    CHECK(r.errors == 0);
    CHECK(r.includes.size() == 4);
    CHECK(r.graph.targets.size() == 3);
    CHECK(r.scan_ms.size() == 2);
    CHECK(r.closure_ms.size() == 2);
    CHECK(r.roots.size() == 2);
    CHECK(r.direct_fan_in[id_of(table, "src/a.hpp")] == 2);
    CHECK(r.transitive_fan_in[id_of(table, "src/c.hpp")] == 2);
    CHECK(r.transitive_fan_in[id_of(table, "README")] == 0);
    CHECK_FALSE(io::failed(r));

    std::ostringstream report;
    io::write_includes_report(report, r, {}, table);
    CHECK_THAT(report.str(),
        Catch::Matchers::ContainsSubstring("unresolved: 1"));
    CHECK_THAT(report.str(),
        Catch::Matchers::ContainsSubstring("<missing.hpp>"));
}

TEST_CASE("includes stage fails loudly when nothing is scanned",
    "[ops][includes_stage]") {
    scoped_git_repo repo;
    repo.write("README", "#include \"a.hpp\"\n");
    repo.track_all();

    const auto table = list(repo.root());
    const auto paths = ops::build_search_paths({}, repo.root(), {}, table);
    const auto r = ops::run_includes_stage(repo.root(), table, paths, 2, 1);

    CHECK(r.files == 0);
    CHECK(io::failed(r));
    std::ostringstream report;
    io::write_includes_report(report, r, {}, table);
    CHECK_THAT(report.str(),
        Catch::Matchers::ContainsSubstring("FAILED: stage 'includes' "
                                           "scanned zero files."));
}

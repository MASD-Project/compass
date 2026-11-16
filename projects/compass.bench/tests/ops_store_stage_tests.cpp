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
#include "compass.bench/ops/store_stage.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.bench/io/report.hpp"
#include "compass.bench/ops/listing.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/io/dump.hpp"
#include "compass.org/io/object.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass::bench;
namespace store = compass::store;

class scoped_git_repo final {
public:
    scoped_git_repo() {
        static int counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("compass.bench.store.tests." + std::to_string(::getpid()) + "." +
                std::to_string(counter++));
        std::filesystem::create_directories(root_ / "checkout");
    }
    ~scoped_git_repo() { std::filesystem::remove_all(root_); }
    scoped_git_repo(const scoped_git_repo&) = delete;
    scoped_git_repo& operator=(const scoped_git_repo&) = delete;

    void write(const std::string& relative, std::string_view content) {
        const auto path = checkout() / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream os(path, std::ios::binary);
        os.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    void track_all() {
        const auto command = "git -C '" + checkout().string() +
            "' init -q && git -C '" + checkout().string() + "' add -A";
        REQUIRE(std::system(command.c_str()) == 0);
    }

    types::file_table list() const {
        const auto raw = ops::git_ls_files(checkout());
        return ops::build_file_table(checkout(), ops::parse_listing(raw));
    }

    std::filesystem::path checkout() const { return root_ / "checkout"; }
    std::filesystem::path objects() const { return root_ / "objects"; }

private:
    std::filesystem::path root_;
};

std::size_t files_under(const std::filesystem::path& directory) {
    std::size_t n = 0;
    for (const auto& entry :
        std::filesystem::recursive_directory_iterator(directory))
        n += entry.is_regular_file() ? 1 : 0;
    return n;
}

constexpr std::string_view heading = "* H\ntext *b* [[id:x]]\n";
constexpr std::string_view listing = "#+title: t\n\n- item\n";

}

TEST_CASE("store stage writes one object per distinct org content",
    "[ops][store_stage]") {
    scoped_git_repo repo;
    repo.write("a.org", heading);
    repo.write("doc/b.org", listing);
    repo.write("doc/copy_of_a.org", heading);
    repo.write("c.cpp", "int x;\n");
    repo.track_all();

    const auto result = ops::run_store_stage(repo.checkout(), repo.list(),
        repo.objects(), 2, 3);

    CHECK(result.cold.files == 3);
    CHECK(result.cold.bytes == 2 * heading.size() + listing.size());
    CHECK(result.cold.errors == 0);
    CHECK(result.cold.wall_ms.size() == 3);
    CHECK(result.map_ms.size() == 3);
    CHECK(result.verify_ms.size() == 3);
    CHECK(result.objects == 2);
    CHECK(result.rejected == 0);
    CHECK(result.mismatches == 0);
    CHECK_FALSE(io::failed(result));
    CHECK(files_under(repo.objects()) == 2);
    const auto a = store::io::object_path(repo.objects(),
        store::ops::blob_id_of(heading));
    const auto b = store::io::object_path(repo.objects(),
        store::ops::blob_id_of(listing));
    REQUIRE(std::filesystem::is_regular_file(a));
    REQUIRE(std::filesystem::is_regular_file(b));
    CHECK(result.object_bytes ==
        std::filesystem::file_size(a) + std::filesystem::file_size(b));
}

TEST_CASE("store stage leaves objects that load as the parse of their source",
    "[ops][store_stage]") {
    scoped_git_repo repo;
    repo.write("a.org", heading);
    repo.track_all();

    const auto result = ops::run_store_stage(repo.checkout(), repo.list(),
        repo.objects(), 1, 1);

    REQUIRE(result.cold.files == 1);
    const auto id = store::ops::blob_id_of(heading);
    const compass::platform::filesystem::mapped_file object(
        store::io::object_path(repo.objects(), id));
    const auto loaded = compass::org::io::decode(object.bytes(),
        {id.bytes, compass::ingest::org::parser_version});
    REQUIRE(loaded.has_value());
    CHECK(compass::org::io::checksum_matches(object.bytes()));
    compass::org::types::tables tables;
    compass::ingest::org::parse(heading, tables);
    std::ostringstream parsed;
    std::ostringstream mapped;
    compass::org::io::dump(parsed, heading,
        compass::org::types::view(tables));
    compass::org::io::dump(mapped, heading, *loaded);
    CHECK(mapped.str() == parsed.str());
    CHECK(loaded->links.size() == 1);
}

TEST_CASE("store stage replaces an object that is not the parse of its key",
    "[ops][store_stage]") {
    scoped_git_repo repo;
    repo.write("a.org", heading);
    repo.track_all();
    const auto path = store::io::object_path(repo.objects(),
        store::ops::blob_id_of(heading));
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << "left by a build that crashed";

    const auto result = ops::run_store_stage(repo.checkout(), repo.list(),
        repo.objects(), 1, 1);

    CHECK(result.rejected == 0);
    CHECK(result.mismatches == 0);
    CHECK(files_under(repo.objects()) == 1);
}

TEST_CASE("store stage counts a deleted but listed file as an error",
    "[ops][store_stage]") {
    scoped_git_repo repo;
    repo.write("kept.org", heading);
    repo.write("gone.org", listing);
    repo.track_all();
    const auto table = repo.list();
    std::filesystem::remove(repo.checkout() / "gone.org");

    const auto result = ops::run_store_stage(repo.checkout(), table,
        repo.objects(), 1, 2);

    CHECK(result.cold.files == 1);
    CHECK(result.cold.errors == 1);
    CHECK(result.objects == 1);
    CHECK(result.rejected == 0);
    CHECK(result.mismatches == 0);
    CHECK(io::failed(result));
    CHECK_THAT(result.cold.first_error,
        Catch::Matchers::ContainsSubstring("gone.org"));
}

TEST_CASE("store report gives the bytes and fails on a mismatch",
    "[io][report]") {
    types::store_result r;
    r.cold.name = "cold";
    r.cold.files = 2;
    r.cold.bytes = 1000;
    r.cold.wall_ms = {4, 2, 3};
    r.map_ms = {1, 1, 1};
    r.verify_ms = {2, 2, 2};
    r.objects = 2;
    r.object_bytes = 1500;
    io::setup_timing setup;
    setup.threads = 2;

    std::ostringstream clean;
    io::write_store_report(clean, r, setup, "/store");
    r.mismatches = 1;
    r.first_mismatch = "doc/x.org";
    std::ostringstream broken;
    io::write_store_report(broken, r, setup, "/store");

    using Catch::Matchers::ContainsSubstring;
    CHECK_THAT(clean.str(), ContainsSubstring(
        "object bytes: 1500  source bytes: 1000  ratio: 1.500"));
    CHECK_THAT(clean.str(), ContainsSubstring("warm verify"));
    CHECK_THAT(clean.str(), !ContainsSubstring("FAILED"));
    CHECK(io::failed(r));
    CHECK_THAT(broken.str(), ContainsSubstring("First: doc/x.org"));
}

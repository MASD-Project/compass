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
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.bench/io/report.hpp"
#include "compass.bench/ops/listing.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/io/dump.hpp"
#include "compass.org/io/object.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.platform/process/run_program.hpp"
#include "compass.store/io/pack_store.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"

namespace {

using namespace compass::bench;
namespace store = compass::store;

class scoped_git_repo final {
    compass::testing::filesystem::scoped_directory dir_{
        "compass.bench.store.tests"};

public:
    scoped_git_repo() {
        std::filesystem::create_directories(dir_.path() / "checkout");
    }

    void write(const std::string& relative, std::string_view content) {
        const auto path = checkout() / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream os(path, std::ios::binary);
        os.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    void track_all() {
        const auto command = "git -C '" + checkout().generic_string() +
            "' init -q && git -C '" + checkout().generic_string() + "' add -A";
        REQUIRE(compass::platform::process::run_program(
            {{"sh", "-c", command}, {}, {}}).exit_code == 0);
    }

    types::file_table list() const {
        const auto raw = ops::git_ls_files(checkout());
        return ops::build_file_table(checkout(), ops::parse_listing(raw));
    }

    std::filesystem::path checkout() const { return dir_.path() / "checkout"; }
    std::filesystem::path packs() const { return dir_.path() / "packs"; }
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

std::optional<std::span<const std::byte>> object_in(
    const store::io::pack_reader& r, std::string_view content) {
    return r.find(store::types::entry_kind::object,
        store::ops::blob_id_of(content));
}

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
        repo.packs(), 2, 3);

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
    CHECK(files_under(repo.packs()) == 1);
    const store::io::pack_reader r(repo.packs());
    const auto a = object_in(r, heading);
    const auto b = object_in(r, listing);
    REQUIRE(a);
    REQUIRE(b);
    CHECK(result.object_bytes == a->size() + b->size());
}

TEST_CASE("store stage leaves objects that load as the parse of their source",
    "[ops][store_stage]") {
    scoped_git_repo repo;
    repo.write("a.org", heading);
    repo.track_all();

    const auto result = ops::run_store_stage(repo.checkout(), repo.list(),
        repo.packs(), 1, 1);

    REQUIRE(result.cold.files == 1);
    const auto id = store::ops::blob_id_of(heading);
    const store::io::pack_reader r(repo.packs());
    const auto object = object_in(r, heading);
    REQUIRE(object);
    const auto loaded = compass::org::io::decode(*object,
        {id.bytes, compass::ingest::org::parser_version});
    REQUIRE(loaded.has_value());
    CHECK(compass::org::io::checksum_matches(*object));
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

TEST_CASE("store stage reads its own object over one that is not the parse "
    "of its key", "[ops][store_stage]") {
    scoped_git_repo repo;
    repo.write("a.org", heading);
    repo.track_all();
    {
        const std::string damaged = "left by a build that crashed";
        store::io::pack_writer w(repo.packs());
        w.add(store::types::entry_kind::object,
            store::ops::blob_id_of(heading), std::as_bytes(std::span(damaged)),
            1);
        REQUIRE(w.publish_only());
    }

    const auto result = ops::run_store_stage(repo.checkout(), repo.list(),
        repo.packs(), 1, 1);

    CHECK(result.rejected == 0);
    CHECK(result.mismatches == 0);
    CHECK(files_under(repo.packs()) == 2);
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
        repo.packs(), 1, 2);

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

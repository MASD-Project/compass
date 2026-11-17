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
#include "compass.cli/io/version.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/ops/action_cache.hpp"
#include "compass.codec/ops/dependencies.hpp"
#include "compass.codec/ops/format_cache.hpp"
#include "compass.codec/ops/outputs.hpp"
#include "compass.store/ops/pack.hpp"
#include "compass.store/ops/worktree_index.hpp"

namespace {

using compass::cli::io::version_text;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;

/**
 * @brief A fresh directory standing in for the bin directory of a build:
 * the path of a compass binary in it and, when written, its manifest.
 */
class scoped_bin final {
public:
    scoped_bin() {
        static int counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("compass.cli.version." + std::to_string(::getpid()) + "." +
                std::to_string(counter++));
        std::filesystem::create_directories(root_);
    }
    ~scoped_bin() { std::filesystem::remove_all(root_); }
    scoped_bin(const scoped_bin&) = delete;
    scoped_bin& operator=(const scoped_bin&) = delete;

    std::filesystem::path executable() const { return root_ / "compass"; }

    /**
     * @brief Writes the executable, last written at 2026-01-02 03:04:05
     * UTC.
     */
    void write_executable() const {
        std::ofstream(executable()) << "binary";
        using namespace std::chrono;
        const auto at = sys_days{2026y / January / 2} + 3h + 4min + 5s;
        std::filesystem::last_write_time(
            executable(), clock_cast<std::filesystem::file_time_type::clock>(at));
    }

    void write_manifest(std::string_view content) const {
        std::ofstream(root_ / compass::cli::io::manifest_name) << content;
    }

private:
    std::filesystem::path root_;
};

constexpr std::string_view clean_manifest =
    "format=1\nversion=0.0.1\nbuild_info=local 1a2b3c4\ncommit=1a2b3c4\n"
    "commit_full=1a2b3c4d5e6f\ncommit_time=2026-01-01T10:00:00+00:00\n"
    "dirty=false\nbranch=feature/x\n";

}

TEST_CASE("version_text reads the manifest beside the executable",
    "[io][version]") {
    scoped_bin bin;
    bin.write_executable();
    bin.write_manifest(clean_manifest);

    const auto text = version_text(bin.executable());

    CHECK_THAT(text, StartsWith("compass "));
    CHECK_THAT(text,
        ContainsSubstring("\ncommit: 1a2b3c4d5e6f (1a2b3c4)\n"
                          "dirty: no\n"
                          "branch: feature/x\n"
                          "committed: 2026-01-01T10:00:00+00:00\n"
                          "built: 2026-01-02T03:04:05Z\n"
                          "installed: no\n"));
}

TEST_CASE("version_text names a dirty tree and the install time",
    "[io][version]") {
    scoped_bin bin;
    bin.write_manifest("format=1\ncommit=1a2b3c4\ncommit_full=1a2b3c4d5e6f\n"
                       "dirty=true\nbranch=main\n"
                       "installed=2026-01-03T00:00:00Z\n");

    const auto text = version_text(bin.executable());

    CHECK_THAT(text, ContainsSubstring("\ndirty: yes\n"));
    CHECK_THAT(text, ContainsSubstring("\ninstalled: 2026-01-03T00:00:00Z\n"));
}

TEST_CASE("version_text gives facts a CI manifest leaves empty as unknown",
    "[io][version]") {
    scoped_bin bin;
    bin.write_manifest("format=1\ncommit=1a2b3c4\ncommit_full=1a2b3c4\n"
                       "commit_time=\ndirty=false\nbranch=\n");

    const auto text = version_text(bin.executable());

    CHECK_THAT(text, ContainsSubstring("\ncommit: 1a2b3c4 (1a2b3c4)\n"));
    CHECK_THAT(text, ContainsSubstring("\nbranch: unknown\n"));
    CHECK_THAT(text, ContainsSubstring("\ncommitted: unknown\n"));
}

TEST_CASE("version_text says unknown without a usable manifest",
    "[io][version]") {
    const std::string unknown = "\ncommit: unknown\ndirty: unknown\n"
                                "branch: unknown\ncommitted: unknown\n";
    scoped_bin bin;
    bin.write_executable();

    SECTION("no manifest") {
        const auto text = version_text(bin.executable());
        CHECK_THAT(text, ContainsSubstring(unknown));
        CHECK_THAT(text, ContainsSubstring("\nbuilt: 2026-01-02T03:04:05Z\n"
                                           "installed: unknown\n"));
    }
    SECTION("no executable path") {
        const auto text = version_text({});
        CHECK_THAT(text, ContainsSubstring(unknown));
        CHECK_THAT(text, ContainsSubstring("\nbuilt: unknown\n"));
    }
    SECTION("not key=value") {
        bin.write_manifest("\x01\x02garbage\n");
        CHECK_THAT(version_text(bin.executable()), ContainsSubstring(unknown));
    }
    SECTION("another format") {
        bin.write_manifest(
            "format=2\ncommit=1a2b3c4\ncommit_full=1a2b3c4d\ndirty=false\n");
        CHECK_THAT(version_text(bin.executable()), ContainsSubstring(unknown));
    }
    SECTION("no full commit") {
        bin.write_manifest("format=1\ncommit=1a2b3c4\ndirty=false\n");
        CHECK_THAT(version_text(bin.executable()), ContainsSubstring(unknown));
    }
    SECTION("a commit git could not name") {
        bin.write_manifest(
            "format=1\ncommit=unknown\ncommit_full=unknown\ndirty=false\n");
        CHECK_THAT(version_text(bin.executable()), ContainsSubstring(unknown));
    }
    SECTION("a dirty flag that is neither true nor false") {
        bin.write_manifest(
            "format=1\ncommit=1a2b3c4\ncommit_full=1a2b3c4d\ndirty=maybe\n");
        CHECK_THAT(version_text(bin.executable()), ContainsSubstring(unknown));
    }
}

TEST_CASE("version_text prints the store formats compiled in",
    "[io][version]") {
    namespace codec = compass::codec::ops;
    namespace store = compass::store::ops;
    const auto n = [](auto v) { return std::to_string(v); };
    scoped_bin bin;

    CHECK_THAT(version_text(bin.executable()),
        ContainsSubstring("\nstore formats: store layout " +
            n(store::store_layout_version) + ", packs " +
            n(store::pack_version) + ", dependency records " +
            n(codec::dependency_record_version) + ", actions " +
            n(codec::action_format_version) + ", written outputs " +
            n(codec::written_outputs_version) + ", format cache " +
            n(codec::format_cache_version) + ", worktree index " +
            n(store::worktree_index_version) + "\n"));
}

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
#include "compass.platform/filesystem/new_file.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include "compass.platform/filesystem/file_lock.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"

namespace {

using compass::testing::filesystem::scoped_directory;

namespace fs = compass::platform::filesystem;

std::string read(const std::filesystem::path& p) {
    std::ifstream is(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(is), {}};
}

std::span<const std::byte> bytes_of(std::string_view s) {
    return std::as_bytes(std::span(s.data(), s.size()));
}

}

TEST_CASE("new_file writes what it is given, byte for byte",
    "[filesystem][new_file]") {
    const scoped_directory dir("compass.platform.new_file");
    const auto path = dir.path() / "a";
    auto file = fs::new_file::create(path);
    REQUIRE(file);
    file->write(bytes_of("one\n"));
    file->write(bytes_of(std::string(100'000, 'x')));
    file->write(bytes_of("\r\nend"));
    file->close();
    CHECK(read(path) == "one\n" + std::string(100'000, 'x') + "\r\nend");
}

TEST_CASE("new_file creates no file that already exists",
    "[filesystem][new_file]") {
    const scoped_directory dir("compass.platform.new_file");
    const auto path = dir.path() / "a";
    std::ofstream(path) << "kept";
    CHECK_FALSE(fs::new_file::create(path));
    CHECK(read(path) == "kept");
}

TEST_CASE("a moved new_file keeps writing to the same file",
    "[filesystem][new_file]") {
    const scoped_directory dir("compass.platform.new_file");
    const auto path = dir.path() / "a";
    auto first = fs::new_file::create(path);
    REQUIRE(first);
    first->write(bytes_of("a"));
    auto second = std::move(*first);
    first.reset();
    second.write(bytes_of("b"));
    second.close();
    CHECK(read(path) == "ab");
}

TEST_CASE("file_lock is exclusive until its holder goes",
    "[filesystem][file_lock]") {
    const scoped_directory dir("compass.platform.new_file");
    const auto path = dir.path() / "lock";
    {
        const fs::file_lock held(path, true);
        CHECK(held.held());
        const fs::file_lock refused(path, false);
        CHECK_FALSE(refused.held());
    }
    const fs::file_lock again(path, false);
    CHECK(again.held());
}

TEST_CASE("an owner-only new_file is readable and writable by its owner "
    "alone", "[filesystem][new_file][posix]") {
    const scoped_directory dir("compass.platform.new_file");
    const auto path = dir.path() / "secrets";
    auto file = fs::new_file::create(path, true);
    REQUIRE(file);
    file->close();
    namespace sfs = std::filesystem;
    CHECK(sfs::status(path).permissions() ==
        (sfs::perms::owner_read | sfs::perms::owner_write));
}

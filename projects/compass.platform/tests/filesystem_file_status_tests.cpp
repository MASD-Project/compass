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
#include "compass.platform/filesystem/file_status.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <catch2/catch_test_macros.hpp>

namespace {

using compass::testing::filesystem::scoped_directory;

namespace fs = compass::platform::filesystem;

/**
 * @brief Sets the modification time of @p file to @p sec seconds and
 * @p nsec nanoseconds after the Unix epoch.
 */
void set_mtime(const std::filesystem::path& file, std::int64_t sec,
    std::uint32_t nsec) {
    fs::set_modified_time(file, {sec, nsec});
}

}

TEST_CASE("regular_file_status gives the size and modification time of a "
    "regular file", "[filesystem][file_status]") {
    const scoped_directory dir("compass.platform.status");
    const auto file = dir.path() / "a";
    std::ofstream(file, std::ios::binary) << "twelve bytes";
    set_mtime(file, 1'700'000'000, 123'456'700);

    const auto s = fs::regular_file_status(file);
    REQUIRE(s);
    CHECK(s->size == 12);
    CHECK(s->modified.seconds == 1'700'000'000);
    CHECK(s->modified.nanoseconds == 123'456'700);
    CHECK(s->modified.count_ns() == 1'700'000'000'123'456'700);
}

TEST_CASE("regular_file_status gives nothing for a directory or a missing "
    "path", "[filesystem][file_status]") {
    const scoped_directory dir("compass.platform.status");
    CHECK_FALSE(fs::regular_file_status(dir.path()));
    CHECK_FALSE(fs::regular_file_status(dir.path() / "missing"));
}

TEST_CASE("read_file reads every byte and the status of the file it read",
    "[filesystem][file_status]") {
    const scoped_directory dir("compass.platform.status");
    const auto file = dir.path() / "a";
    const std::string content(10'000, 'x');
    std::ofstream(file, std::ios::binary) << content << "\r\n";
    set_mtime(file, 1'600'000'000, 0);

    std::string bytes = "left over";
    const auto s = fs::read_file(file, bytes);
    CHECK(bytes == content + "\r\n");
    CHECK(s.size == bytes.size());
    CHECK(s.modified.seconds == 1'600'000'000);
}

TEST_CASE("read_file throws ENOENT naming a missing path",
    "[filesystem][file_status]") {
    const scoped_directory dir("compass.platform.status");
    std::string bytes;
    try {
        fs::read_file(dir.path() / "missing", bytes);
        FAIL("expected std::system_error");
    } catch (const std::system_error& e) {
        CHECK(e.code() == std::errc::no_such_file_or_directory);
    }
}

TEST_CASE("read_file refuses a directory before reading it",
    "[filesystem][file_status]") {
    const scoped_directory dir("compass.platform.status");
    std::string bytes;
    try {
        fs::read_file(dir.path(), bytes);
        FAIL("expected std::system_error");
    } catch (const std::system_error& e) {
        CHECK(e.code() == std::errc::invalid_argument);
    }
}

TEST_CASE("file_clock_now is the time of day", "[filesystem][file_status]") {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto t = fs::file_clock_now();
    CHECK(t.seconds >= now - 1);
    CHECK(t.seconds <= now + 1);
    CHECK(t.nanoseconds < 1'000'000'000);
}

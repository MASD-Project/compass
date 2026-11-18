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
#include "compass.testing/filesystem/scoped_directory.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.platform/process/environment.hpp"

namespace {

using compass::testing::filesystem::scoped_directory;

const std::string prefix = "compass.testing.tests";

std::filesystem::path temp() {
    return std::filesystem::canonical(std::filesystem::temp_directory_path());
}

std::string self() {
    return std::to_string(compass::platform::process::current_process_id());
}

/**
 * @brief Creates @p name under the temporary directory, holding a file, as
 * a run that was killed leaves it.
 */
std::filesystem::path leave_behind(const std::string& name) {
    const auto p = temp() / name;
    std::filesystem::create_directories(p / "sub");
    std::ofstream(p / "sub" / "file") << "left";
    return p;
}

/**
 * @brief An ID no process has: past every POSIX pid_t, and never a
 * Windows process ID, which is a multiple of four.
 */
const std::string dead = "4294967293";

}

TEST_CASE("a scoped directory is new, names its suite and process, and is "
    "removed with its contents", "[filesystem][scoped_directory]") {
    std::filesystem::path path;
    {
        const scoped_directory d(prefix);
        path = d.path();
        CHECK(path.is_absolute());
        CHECK(path.parent_path() == temp());
        CHECK(path.filename().string().starts_with(prefix + "." + self() + "."));
        REQUIRE(std::filesystem::is_directory(path));
        CHECK(std::filesystem::is_empty(path));
        std::filesystem::create_directories(path / "a" / "b");
        std::ofstream(path / "a" / "b" / "c") << "c";
    }
    CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("two scoped directories alive at once are distinct",
    "[filesystem][scoped_directory]") {
    const scoped_directory a(prefix);
    const scoped_directory b(prefix);
    CHECK(a.path() != b.path());
}

TEST_CASE("a fixture that throws after making its directory removes it",
    "[filesystem][scoped_directory]") {
    struct fixture {
        scoped_directory dir{prefix};
        explicit fixture(std::filesystem::path& made) {
            made = dir.path();
            throw std::runtime_error("git failed");
        }
    };
    std::filesystem::path made;
    CHECK_THROWS_AS(fixture(made), std::runtime_error);
    REQUIRE_FALSE(made.empty());
    CHECK_FALSE(std::filesystem::exists(made));
}

TEST_CASE("a scoped directory removes those its prefix's dead processes left",
    "[filesystem][scoped_directory]") {
    const auto orphan = leave_behind(prefix + "." + dead + ".0");
    const scoped_directory d(prefix);
    CHECK_FALSE(std::filesystem::exists(orphan));
}

TEST_CASE("a scoped directory leaves this process's and other prefixes'",
    "[filesystem][scoped_directory]") {
    const auto names = {
        prefix + ".other." + dead + ".0",
        prefix + "x." + dead + ".0",
        prefix + "." + dead,
        prefix + "." + dead + ".0x",
        prefix + "." + self() + ".999999999"};
    for (const auto& name : names)
        leave_behind(name);
    { const scoped_directory d(prefix); }
    for (const auto& name : names) {
        CHECK(std::filesystem::exists(temp() / name));
        std::filesystem::remove_all(temp() / name);
    }
}

TEST_CASE("a scoped directory leaves those of a running process",
    "[filesystem][scoped_directory][posix]") {
    const auto of_init = leave_behind(prefix + ".1.0");
    { const scoped_directory d(prefix); }
    CHECK(std::filesystem::exists(of_init));
    std::filesystem::remove_all(of_init);
}

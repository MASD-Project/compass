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
#include "compass.platform/process/run_program.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>

namespace {

using compass::platform::process::run_program;

class scoped_temp_dir final {
public:
    scoped_temp_dir() {
        path_ = std::filesystem::temp_directory_path() /
            ("compass.platform.process." + std::to_string(::getpid()));
        std::filesystem::create_directories(path_);
    }
    ~scoped_temp_dir() { std::filesystem::remove_all(path_); }
    scoped_temp_dir(const scoped_temp_dir&) = delete;
    scoped_temp_dir& operator=(const scoped_temp_dir&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

}

TEST_CASE("run_program returns the standard output of a program found on PATH",
    "[process]") {
    const auto r = run_program({{"printf", "a\\0b"}, {}, {}});
    CHECK(r.exit_code == 0);
    CHECK(r.output == std::string("a\0b", 3));
}

TEST_CASE("run_program reports a failing exit code instead of throwing",
    "[process]") {
    const auto r = run_program({{"sh", "-c", "echo out; exit 3"}, {}, {}});
    CHECK(r.exit_code == 3);
    CHECK(r.output == "out\n");
}

TEST_CASE("run_program runs the program in the directory it is given",
    "[process]") {
    const scoped_temp_dir dir;
    const auto r = run_program({{"pwd", "-P"}, dir.path(), {}});
    CHECK(r.output == std::filesystem::canonical(dir.path()).string() + "\n");
}

TEST_CASE("run_program feeds the input file to standard input", "[process]") {
    const scoped_temp_dir dir;
    const auto input = dir.path() / "input";
    std::ofstream(input) << "fed";
    const auto r = run_program({{"cat"}, {}, input});
    CHECK(r.output == "fed");
}

TEST_CASE("run_program throws when the program cannot be started",
    "[process]") {
    CHECK_THROWS_AS(run_program({{"compass-no-such-program"}, {}, {}}),
        std::system_error);
}

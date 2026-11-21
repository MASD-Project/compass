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
#include "compass.testing/filesystem/scoped_directory.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

namespace {

using compass::testing::filesystem::scoped_directory;
using compass::platform::process::error_stream;
using compass::platform::process::run_program;

}

TEST_CASE("run_program returns the standard output of a program found on PATH",
    "[process]") {
    const auto r = run_program({{"sh", "-c", "printf 'a\\0b'"}, {}, {}});
    CHECK(r.exit_code == 0);
    CHECK(r.output == std::string("a\0b", 3));
}

TEST_CASE("run_program passes each argument through unchanged", "[process]") {
    const auto r = run_program({{"git", "rev-parse", "--sq-quote", "a b",
        "tab\there", "quote\"d", "back\\slash", "trailing\\", "",
        "\\\"mixed\\\\\""}, {}, {}});
    CHECK(r.exit_code == 0);
    CHECK(r.output == " 'a b' 'tab\there' 'quote\"d' 'back\\slash' "
        "'trailing\\' '' '\\\"mixed\\\\\"'\n");
}

TEST_CASE("run_program reports a failing exit code instead of throwing",
    "[process]") {
    const auto r = run_program({{"sh", "-c", "echo out; exit 3"}, {}, {}});
    CHECK(r.exit_code == 3);
    CHECK(r.output == "out\n");
}

TEST_CASE("run_program runs the program in the directory it is given",
    "[process]") {
    const scoped_directory dir("compass.platform.process");
    std::ofstream(dir.path() / "marker") << "here";
    const auto r = run_program({{"sh", "-c", "cat marker"}, dir.path(), {}});
    CHECK(r.exit_code == 0);
    CHECK(r.output == "here");
}

TEST_CASE("run_program feeds the input file to standard input", "[process]") {
    const scoped_directory dir("compass.platform.process");
    const auto input = dir.path() / "input";
    std::ofstream(input) << "fed";
    const auto r = run_program({{"sh", "-c", "cat"}, {}, input});
    CHECK(r.output == "fed");
}

TEST_CASE("run_program passes braces and carets through, unexpanded",
    "[process]") {
    const auto r = run_program({{"git", "rev-parse", "--sq-quote",
        "main^{commit}", "{a,b}", "*"}, {}, {}});
    CHECK(r.exit_code == 0);
    CHECK(r.output == " 'main^{commit}' '{a,b}' '*'\n");
}

TEST_CASE("run_program discards standard error, or merges it into the "
    "output, as the call says", "[process]") {
    const std::vector<std::string> failing{"git", "rev-parse", "--verify",
        "compass-no-such-revision"};
    const auto discarded = run_program({failing, {}, {},
        error_stream::discard});
    CHECK(discarded.exit_code != 0);
    CHECK(discarded.output.empty());
    const auto merged = run_program({failing, {}, {}, error_stream::merge});
    CHECK(merged.exit_code != 0);
    CHECK_THAT(merged.output, Catch::Matchers::ContainsSubstring("fatal"));
}

TEST_CASE("run_program throws when the program cannot be started",
    "[process]") {
    CHECK_THROWS_AS(run_program({{"compass-no-such-program"}, {}, {}}),
        std::system_error);
    try {
        const auto r = run_program({{"compass-no-such-program"}, {}, {}});
        FAIL("ran with exit code " << r.exit_code);
    } catch (const std::system_error& e) {
        CHECK(e.code() == std::errc::no_such_file_or_directory);
    }
}

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
#include "compass.bench/io/compile_commands.hpp"

#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>

using namespace compass::bench;

TEST_CASE("split_command honours shell quoting", "[io][compile_commands]") {
    const auto words = io::split_command(
        R"(clang++  -DA="x y" '-DB=$q' -DC=\"z\" "-I/a b" -DD="p\q")");

    const std::vector<std::string> expected = {"clang++", "-DA=x y",
        "-DB=$q", "-DC=\"z\"", "-I/a b", "-DD=p\\q"};
    CHECK(words == expected);
}

TEST_CASE("parse_compile_commands reads command and arguments entries",
    "[io][compile_commands]") {
    const auto commands = io::parse_compile_commands(R"([
        {"directory": "/b", "file": "src/a.cpp",
         "command": "c++ -I/inc -c src/a.cpp", "output": "a.o"},
        {"file": "/s/b.cpp", "extra": {"k": [1, true, null, -2.5e3]},
         "arguments": ["c++", "-isystem", "/sysé", "-c"],
         "directory": "/b"}
    ])");

    REQUIRE(commands.size() == 2);
    CHECK(commands[0].directory == "/b");
    CHECK(commands[0].file == "/b/src/a.cpp");
    CHECK(commands[0].arguments ==
        std::vector<std::string>{"c++", "-I/inc", "-c", "src/a.cpp"});
    CHECK(commands[1].file == "/s/b.cpp");
    CHECK(commands[1].arguments ==
        std::vector<std::string>{"c++", "-isystem", "/sys\xc3\xa9", "-c"});
}

TEST_CASE("parse_compile_commands rejects malformed databases",
    "[io][compile_commands]") {
    CHECK_THROWS(io::parse_compile_commands(R"([{"directory": "/b")"));
    CHECK_THROWS(io::parse_compile_commands(R"([{"directory": "/b",
        "file": "a.cpp"}])"));
    CHECK_THROWS(io::parse_compile_commands(R"([] x)"));
}

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
#include "compass.cpp/codecs/unity_collisions.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"CMakeLists.txt", ""},
        {"projects/ores.a/src/CMakeLists.txt", ""},
        {"projects/ores.a/src/one.cpp",
         "namespace ores::a {\n"
         "namespace {\n"
         "int helper(int x) { return x; }\n"
         "}\n"
         "static int counter = 0;\n"
         "inline const int shared = 1;\n"
         "// static int commented = 0;\n"
         "}\n"},
        {"projects/ores.a/src/sub/two.cpp",
         "namespace ores::a {\n"
         "\n"
         "static int helper(int y) { return y; }\n"
         "const int counter = 2;\n"
         "inline const int shared = 1;\n"
         "static int commented = 0;\n"
         "}\n"},
        {"projects/ores.b/src/CMakeLists.txt", ""},
        {"projects/ores.b/src/three.cpp",
         "namespace ores::a {\nstatic int helper(int z) { return z; }\n}\n"},
        {"projects/ores.a/src/other.cpp",
         "namespace ores::other {\nstatic int helper() { return 0; }\n}\n"}};
}

}

TEST_CASE("names two sources of one target define with internal linkage "
    "collide", "[codecs][unity_collisions]") {
    const auto r = tests::run(codecs::unity_collisions, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.a/src/one.cpp", 5},
        {"projects/ores.a/src/one.cpp", 3}});
}

TEST_CASE("a recorded file is out of the unity build, so its names do not "
    "collide", "[codecs][unity_collisions]") {
    auto t = tree();
    t["cmake/unity_exclusions.cmake"] =
        "set_source_files_properties(\n"
        "    \"${CMAKE_SOURCE_DIR}/projects/ores.a/src/sub/two.cpp\"\n"
        "    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)\n";
    CHECK(tests::run(codecs::unity_collisions, t).findings.empty());
}

TEST_CASE("a recorded file that does not exist is reported",
    "[codecs][unity_collisions]") {
    auto t = tree();
    t["cmake/unity_exclusions.cmake"] =
        "set_source_files_properties(\n"
        "    \"${CMAKE_SOURCE_DIR}/projects/ores.a/src/sub/two.cpp\"\n"
        "    \"${CMAKE_SOURCE_DIR}/projects/ores.a/src/gone.cpp\"\n"
        "    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)\n";
    CHECK(tests::places(tests::run(codecs::unity_collisions, t).findings) ==
        places{{"cmake/unity_exclusions.cmake", 3}});
}

TEST_CASE("a file that is not UTF-8 is skipped, as read_text() fails on it",
    "[codecs][unity_collisions]") {
    auto t = tree();
    t["projects/ores.a/src/sub/two.cpp"] += "// \xff\n";
    CHECK(tests::run(codecs::unity_collisions, t).findings.empty());
}

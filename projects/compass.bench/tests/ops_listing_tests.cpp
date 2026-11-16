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
#include "compass.bench/ops/listing.hpp"

#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace std::string_view_literals;
using compass::bench::ops::add_file;
using compass::bench::ops::extension_of;
using compass::bench::ops::parse_listing;

}

TEST_CASE("parse_listing of an empty listing yields no paths",
    "[ops][listing]") {
    CHECK(parse_listing(""sv).empty());
}

TEST_CASE("parse_listing splits on NUL and keeps spaces in paths",
    "[ops][listing]") {
    const auto raw = "a.cpp\0dir/with space.hpp\0x\0"sv;

    const auto paths = parse_listing(raw);

    const std::vector<std::string_view> expected{
        "a.cpp"sv, "dir/with space.hpp"sv, "x"sv};
    CHECK(paths == expected);
}

TEST_CASE("parse_listing keeps a final path that lacks its terminator",
    "[ops][listing]") {
    const auto paths = parse_listing("a\0b"sv);

    const std::vector<std::string_view> expected{"a"sv, "b"sv};
    CHECK(paths == expected);
}

TEST_CASE("extension_of reads only the last path segment",
    "[ops][listing]") {
    CHECK(extension_of("src/a.cpp") == ".cpp");
    CHECK(extension_of("a.tar.gz") == ".gz");
    CHECK(extension_of("dir.d/Makefile").empty());
    CHECK(extension_of("dir/.gitignore").empty());
    CHECK(extension_of("README").empty());
}

TEST_CASE("add_file interns each extension once", "[ops][listing]") {
    compass::bench::types::file_table table;

    add_file(table, "a.cpp", 10);
    add_file(table, "b.hpp", 20);
    add_file(table, "dir/c.cpp", 30);
    add_file(table, "Makefile", 40);
    add_file(table, ".gitignore", 50);

    REQUIRE(table.size() == 5);
    CHECK(table.extensions == std::vector<std::string>{".cpp", ".hpp", ""});
    CHECK(table.extension_ids == std::vector<std::uint32_t>{0, 1, 0, 2, 2});
    CHECK(table.sizes == std::vector<std::uint64_t>{10, 20, 30, 40, 50});
    CHECK(table.paths[2] == "dir/c.cpp");
}

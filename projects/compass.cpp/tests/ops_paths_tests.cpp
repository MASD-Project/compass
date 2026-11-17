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
#include "compass.cpp/ops/paths.hpp"

#include <catch2/catch_test_macros.hpp>
#include "compass.cpp/ops/text.hpp"

using namespace compass::cpp::ops;

TEST_CASE("a glob matches component by component, ** any depth",
    "[ops][paths]") {
    CHECK(glob_matches("projects/**/modeling/*.org",
        "projects/ores.a/modeling/x.org"));
    CHECK(glob_matches("projects/**/modeling/*.org",
        "projects/ores.a/core/modeling/x.org"));
    CHECK(glob_matches("projects/**/modeling/*.org",
        "projects/modeling/x.org"));
    CHECK_FALSE(glob_matches("projects/**/modeling/*.org",
        "projects/ores.a/modeling/sub/x.org"));
    CHECK(glob_matches("projects/ores.*/api/include/*/messaging/*_protocol.hpp",
        "projects/ores.iam/api/include/ores.iam.api/messaging/a_protocol.hpp"));
    CHECK_FALSE(glob_matches("projects/*/api/src/domain/*_table.cpp",
        "projects/ores.a/b/api/src/domain/x_table.cpp"));
    CHECK(glob_matches("a/?.txt", "a/b.txt"));
    CHECK_FALSE(glob_matches("a/?.txt", "a/bc.txt"));
    CHECK(glob_matches("a/*", "a/.hidden"));
}

TEST_CASE("split_lines_keeping_ends keeps each line's separator",
    "[ops][text]") {
    CHECK(split_lines_keeping_ends("a\r\nb\rc\x0b" "d\ne") ==
        std::vector<std::string_view>{"a\r\n", "b\r", "c\x0b", "d\n", "e"});
}

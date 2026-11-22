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
#include "compass.sql/ops/includes.hpp"

#include <array>
#include <map>
#include <string>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::sql;

const std::map<std::string, std::string, std::less<>> scripts{
    {"projects/ores.sql/setup.sql",
     "select 1;\n"
     "\\ir create/a.sql\n"
     "select 2;\n"
     "\\ir ./populate/b.sql\n"
     "\\ir missing.sql\n"},
    {"projects/ores.sql/create/a.sql", "\\i populate/c.sql\n"},
    {"projects/ores.sql/populate/b.sql",
     "select 3;\n\\ir ../create/a.sql\n\\ir c.sql\n"},
    {"projects/ores.sql/populate/c.sql", "select 4;\n"},
    {"projects/ores.sql/drop/drop.sql", ""},
    {"projects/ores.sql/drop/orphan.sql", ""}};

ops::include_walk walk() {
    const std::array<std::string_view, 1> roots{
        "projects/ores.sql/setup.sql"};
    return ops::walk_includes(roots,
        [](std::string_view p) {
            const auto& text = scripts.find(p)->second;
            return ops::model_of(text).includes;
        },
        [](std::string_view p) { return scripts.contains(p); });
}

}

TEST_CASE("the walk runs each file where psql first reaches it, between "
    "the statements of the file that includes it", "[ops][includes]") {
    const auto w = walk();
    const auto at = [&](std::string_view path, std::uint32_t offset) {
        return *w.when("projects/ores.sql/" + std::string(path), offset);
    };
    CHECK(at("setup.sql", 0) < at("create/a.sql", 0));
    CHECK(at("create/a.sql", 0) < at("populate/c.sql", 0));
    CHECK(at("populate/c.sql", 0) < at("setup.sql", 27));
    CHECK(at("setup.sql", 27) < at("populate/b.sql", 0));
    CHECK(at("populate/b.sql", 0) < at("setup.sql", 60));
    CHECK(at("setup.sql", 3) < at("setup.sql", 5));
    CHECK_FALSE(w.when("projects/ores.sql/drop/orphan.sql", 0));
}

TEST_CASE("the walk reports each include and entry point that names no "
    "file", "[ops][includes]") {
    const auto w = walk();
    REQUIRE(w.missing_includes.size() == 1);
    CHECK(w.missing_includes[0].file == "projects/ores.sql/setup.sql");
    CHECK(w.missing_includes[0].names == "missing.sql");
    CHECK(w.missing_includes[0].offset == 58);
    CHECK(w.missing_entry_points.size() == 3);
    CHECK(w.reached("projects/ores.sql/drop/drop.sql"));
}

TEST_CASE("an include of a psql variable is not followed or reported "
    "missing", "[ops][includes]") {
    const std::string setup = "\\if :{?pages}\n\\i :pages\n\\endif\n"
        "\\ir :'more'\n\\ir missing.sql\n";
    REQUIRE(ops::model_of(setup).includes.size() == 3);
    const std::array<std::string_view, 1> roots{"projects/ores.sql/setup.sql"};
    const auto w = ops::walk_includes(roots,
        [&](std::string_view) { return ops::model_of(setup).includes; },
        [](std::string_view p) { return p == "projects/ores.sql/setup.sql"; });
    REQUIRE(w.missing_includes.size() == 1);
    CHECK(w.missing_includes[0].names == "missing.sql");
}

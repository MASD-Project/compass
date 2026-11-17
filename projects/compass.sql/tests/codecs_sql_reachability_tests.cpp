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
#include "compass.sql/codecs/sql_reachability.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::sql;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"projects/ores.sql/setup.sql",
         "\\i create/create.sql\n"
         "\\ir populate/populate.sql\n"},
        {"projects/ores.sql/create/create.sql",
         "\\ir tables/a.sql\n"
         "  \\i create/tables/../tables/b.sql\n"
         "\\ir tables/gone.sql\n"},
        {"projects/ores.sql/create/tables/a.sql", "create table a ();\n"},
        {"projects/ores.sql/create/tables/b.sql", "create table b ();\n"},
        {"projects/ores.sql/create/tables/orphan.sql", "create table c ();\n"},
        {"projects/ores.sql/populate/populate.sql", "select 1;\n"},
        {"projects/ores.sql/populate/ip2country/ip2country_import.sql", ""},
        {"projects/ores.sql/drop/drop.sql", "\\ir drop_a.sql\n"},
        {"projects/ores.sql/drop/drop_a.sql", ""},
        {"projects/ores.sql/drop/drop_database.sql", ""},
        {"projects/ores.sql/utility/notes.sql", ""}};
}

}

TEST_CASE("a file nothing includes, an include of nothing and a missing "
    "entry point are reported", "[codecs][sql_reachability]") {
    const auto r = tests::run(codecs::sql_reachability, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.sql", 0},
        {"projects/ores.sql/create/create.sql", 3},
        {"projects/ores.sql/create/tables/orphan.sql", 1}});
}

TEST_CASE("an edit that drops an include leaves its target unreachable, "
    "and one that restores it reaches it again",
    "[codecs][sql_reachability]") {
    auto dropped = tree();
    dropped["projects/ores.sql/create/create.sql"] =
        "\\ir tables/a.sql\n"
        "\\ir tables/gone.sql\n";

    CHECK(tests::places(tests::run_after_edit(codecs::sql_reachability,
        tree(), dropped).findings) == places{
            {"projects/ores.sql", 0},
            {"projects/ores.sql/create/create.sql", 2},
            {"projects/ores.sql/create/tables/b.sql", 1},
            {"projects/ores.sql/create/tables/orphan.sql", 1}});
    CHECK(tests::run_after_edit(codecs::sql_reachability, dropped, tree())
        .findings.size() == 3);
}

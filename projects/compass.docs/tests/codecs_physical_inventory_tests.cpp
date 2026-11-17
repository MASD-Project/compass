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
#include <catch2/catch_test_macros.hpp>

#include <string>
#include "compass.docs/codecs/physical_inventory.hpp"
#include "test_tree.hpp"

namespace {

using namespace compass::docs;

const std::string dir = "projects/ores.codegen/library/templates/";

std::string node(std::string_view id, std::string_view title,
    std::string_view type, std::string_view extra, std::string_view body) {
    return ":PROPERTIES:\n:ID: " + std::string(id) + "\n:END:\n#+title: " +
        std::string(title) + "\n#+type: " + std::string(type) + "\n" +
        std::string(extra) + std::string(body);
}

}

TEST_CASE("each parent lists its children by title with their counts",
    "[codecs][physical_inventory]") {
    const auto out = tests::generate(codecs::physical_inventory, {
        {dir + "ores.org", node("R", "ores", "technical_space", "",
            "\n* Technical spaces\n\nIntro.\n\n| old |\n|-----|\n\n"
            "Outro.\n\n* See also\n")},
        {dir + "ores.cpp.org", node("C", "ores.cpp", "technical_space",
            "#+description: C++\n", "\n* See also\n")},
        {dir + "ores.cpp.b.org", node("FB", "ores.cpp.b", "facet",
            "#+facet_group: ores.cpp\n#+description: B\n", "")},
        {dir + "ores.cpp.a.org", node("FA", "ores.cpp.a", "facet",
            "#+facet_group: ores.cpp\n", "\n* Archetypes\n\nNone yet.\n")},
        {dir + "x.org", node("X", "ores.cpp.a.x", "archetype",
            "#+facet: ores.cpp.a\n#+description: An x\n", "")}});

    CHECK(out.at(dir + "ores.org").ends_with("\n* Technical spaces\n\n"
        "Intro.\n\n| Technical space | Facets | Description |\n"
        "|-----------------+--------+-------------|\n"
        "| [[id:C][ores.cpp]] | 2 | C++ |\n\nOutro.\n\n* See also\n"));
    CHECK(out.at(dir + "ores.cpp.org").ends_with("#+description: C++\n\n"
        "* Facets\n\n| Facet | Archetypes | Description |\n"
        "|-------+------------+-------------|\n"
        "| [[id:FA][ores.cpp.a]] | 1 |  |\n| [[id:FB][ores.cpp.b]] | 0 | B |"
        "\n\n* See also\n"));
    // The last section ends the file: its table is appended once, with no
    // newline left over from where the section ended.
    CHECK(out.at(dir + "ores.cpp.a.org").ends_with("* Archetypes\n\n"
        "None yet.\n\n| Archetype | Description |\n"
        "|-----------+-------------|\n| [[id:X][ores.cpp.a.x]] | An x |\n"));
    CHECK_FALSE(out.contains(dir + "x.org"));
}

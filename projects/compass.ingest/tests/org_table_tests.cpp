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
#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include "dump_of.hpp"

// Each expected dump is what org-element 9.7 prints for the source, taken
// with build/scripts/org_element_dump.el.

using compass::ingest::tests::dump_of;

TEST_CASE("rows, a rule and cells", "[org][table]") {
    const std::string_view source =
        "| a | b |\n"
        "|---+---|\n"
        "| 1 | 2 |\n";
    const std::string_view expected =
        "org-data 0 30 0 30 0\n"
        "  section 0 30 0 30 0\n"
        "    table 0 30 0 30 0 type=org\n"
        "      table-row 0 10 1 9 0 type=standard\n"
        "        table-cell 1 5 2 3 0\n"
        "        table-cell 5 9 6 7 0\n"
        "      table-row 10 20 - - 0 type=rule\n"
        "      table-row 20 30 21 29 0 type=standard\n"
        "        table-cell 21 25 22 23 0\n"
        "        table-cell 25 29 26 27 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a row without a closing bar", "[org][table]") {
    const std::string_view source =
        "| a | b\n";
    const std::string_view expected =
        "org-data 0 8 0 8 0\n"
        "  section 0 8 0 8 0\n"
        "    table 0 8 0 8 0 type=org\n"
        "      table-row 0 8 1 7 0 type=standard\n"
        "        table-cell 1 5 2 3 0\n"
        "        table-cell 5 7 6 7 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("empty cells and an empty row", "[org][table]") {
    const std::string_view source =
        "| | a |  |\n"
        "|\n";
    const std::string_view expected =
        "org-data 0 13 0 13 0\n"
        "  section 0 13 0 13 0\n"
        "    table 0 13 0 13 0 type=org\n"
        "      table-row 0 11 1 10 0 type=standard\n"
        "        table-cell 1 3 - - 0\n"
        "        table-cell 3 7 4 5 0\n"
        "        table-cell 7 10 - - 0\n"
        "      table-row 11 13 - - 0 type=standard\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("formula lines belong to the table", "[org][table]") {
    const std::string_view source =
        "| a |\n"
        "#+TBLFM: $1=1\n"
        "#+tblfm: $2=2\n"
        "\n"
        "x\n";
    const std::string_view expected =
        "org-data 0 37 0 37 0\n"
        "  section 0 37 0 37 0\n"
        "    table 0 35 0 6 1 type=org\n"
        "      table-row 0 6 1 5 0 type=standard\n"
        "        table-cell 1 5 2 3 0\n"
        "    paragraph 35 37 35 37 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a table may be indented", "[org][table]") {
    const std::string_view source =
        "  | a |\n"
        "  |---|\n";
    const std::string_view expected =
        "org-data 0 16 0 16 0\n"
        "  section 0 16 0 16 0\n"
        "    table 0 16 0 16 0 type=org\n"
        "      table-row 0 8 3 7 0 type=standard\n"
        "        table-cell 3 7 4 5 0\n"
        "      table-row 8 16 - - 0 type=rule\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("cells hold objects", "[org][table]") {
    const std::string_view source =
        "| *b* | =v= | [[id:x][d]] | a_b |\n";
    const std::string_view expected =
        "org-data 0 34 0 34 0\n"
        "  section 0 34 0 34 0\n"
        "    table 0 34 0 34 0 type=org\n"
        "      table-row 0 34 1 33 0 type=standard\n"
        "        table-cell 1 7 2 5 0\n"
        "          bold 2 5 3 4 0\n"
        "        table-cell 7 13 8 11 0\n"
        "          verbatim 8 11 - - 0\n"
        "        table-cell 13 27 14 25 0\n"
        "          link 14 25 22 23 0 type=id format=bracket path=x\n"
        "        table-cell 27 33 28 31 0\n"
        "          subscript 29 31 30 31 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a bar inside verbatim still splits the cell", "[org][table]") {
    const std::string_view source =
        "| =a|b= |\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    table 0 10 0 10 0 type=org\n"
        "      table-row 0 10 1 9 0 type=standard\n"
        "        table-cell 1 5 2 4 0\n"
        "        table-cell 5 9 5 7 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("text after a table starts a paragraph", "[org][table]") {
    const std::string_view source =
        "| a |\n"
        "x\n";
    const std::string_view expected =
        "org-data 0 8 0 8 0\n"
        "  section 0 8 0 8 0\n"
        "    table 0 6 0 6 0 type=org\n"
        "      table-row 0 6 1 5 0 type=standard\n"
        "        table-cell 1 5 2 3 0\n"
        "    paragraph 6 8 6 8 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a table.el table has no rows", "[org][table]") {
    const std::string_view source =
        "+---+---+\n"
        "| a | b |\n"
        "+---+---+\n"
        "\n"
        "x\n";
    const std::string_view expected =
        "org-data 0 33 0 33 0\n"
        "  section 0 33 0 33 0\n"
        "    table 0 31 - - 1 type=table.el\n"
        "    paragraph 31 33 31 33 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a lone border line is a paragraph", "[org][table]") {
    const std::string_view source =
        "+---+\n";
    const std::string_view expected =
        "org-data 0 6 0 6 0\n"
        "  section 0 6 0 6 0\n"
        "    paragraph 0 6 0 6 0\n"
        "      strike-through 0 5 1 4 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("rule rows start with a bar and a dash", "[org][table]") {
    const std::string_view source =
        "|-\n"
        "|-----\n"
        "| - |\n";
    const std::string_view expected =
        "org-data 0 16 0 16 0\n"
        "  section 0 16 0 16 0\n"
        "    table 0 16 0 16 0 type=org\n"
        "      table-row 0 3 - - 0 type=rule\n"
        "      table-row 3 10 - - 0 type=rule\n"
        "      table-row 10 16 11 15 0 type=standard\n"
        "        table-cell 11 15 12 13 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an entity inside a cell", "[org][table]") {
    const std::string_view source =
        "| a \\vert b |\n";
    const std::string_view expected =
        "org-data 0 14 0 14 0\n"
        "  section 0 14 0 14 0\n"
        "    table 0 14 0 14 0 type=org\n"
        "      table-row 0 14 1 13 0 type=standard\n"
        "        table-cell 1 13 2 11 0\n"
        "          entity 4 10 - - 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("blank lines after a table are its post-blank", "[org][table]") {
    const std::string_view source =
        "| a |\n"
        "\n"
        "\n"
        "x\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    table 0 8 0 6 2 type=org\n"
        "      table-row 0 6 1 5 0 type=standard\n"
        "        table-cell 1 5 2 3 0\n"
        "    paragraph 8 10 8 10 0\n";
    CHECK(dump_of(source) == expected);
}

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

TEST_CASE("a property drawer opens the document", "[org][drawer]") {
    const std::string_view source =
        ":PROPERTIES:\n"
        ":ID: x\n"
        ":END:\n"
        "#+title: t\n";
    const std::string_view expected =
        "org-data 0 37 0 37 0\n"
        "  section 0 37 0 37 0\n"
        "    property-drawer 0 26 13 20 0\n"
        "      node-property 13 20 - - 0 key=ID value=x\n"
        "    keyword 26 37 - - 0 key=title value=t\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a property drawer may follow a comment at the top",
    "[org][drawer]") {
    const std::string_view source =
        "# c\n"
        ":PROPERTIES:\n"
        ":A: 1\n"
        ":END:\n";
    const std::string_view expected =
        "org-data 0 29 0 29 0\n"
        "  section 0 29 0 29 0\n"
        "    comment 0 4 - - 0\n"
        "    property-drawer 4 29 17 23 0\n"
        "      node-property 17 23 - - 0 key=A value=1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("blank lines before the first drawer keep it a property drawer",
    "[org][drawer]") {
    const std::string_view source =
        "\n"
        "\n"
        ":PROPERTIES:\n"
        ":A: 1\n"
        ":END:\n";
    const std::string_view expected =
        "org-data 0 27 2 27 0\n"
        "  section 2 27 2 27 0\n"
        "    property-drawer 2 27 15 21 0\n"
        "      node-property 15 21 - - 0 key=A value=1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a blank line between the top comment and the drawer makes "
          "it a drawer",
    "[org][drawer]") {
    const std::string_view source =
        "# c\n"
        "\n"
        ":PROPERTIES:\n"
        ":A: 1\n"
        ":END:\n";
    const std::string_view expected =
        "org-data 0 30 0 30 0\n"
        "  section 0 30 0 30 0\n"
        "    comment 0 5 - - 1\n"
        "    drawer 5 30 18 24 0\n"
        "      paragraph 18 24 18 24 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a properties drawer after a keyword is a plain drawer",
    "[org][drawer]") {
    const std::string_view source =
        "#+title: t\n"
        ":PROPERTIES:\n"
        ":A: 1\n"
        ":END:\n";
    const std::string_view expected =
        "org-data 0 36 0 36 0\n"
        "  section 0 36 0 36 0\n"
        "    keyword 0 11 - - 0 key=title value=t\n"
        "    drawer 11 36 24 30 0\n"
        "      paragraph 24 30 24 30 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a property drawer follows its heading", "[org][drawer]") {
    const std::string_view source =
        "* H\n"
        ":PROPERTIES:\n"
        ":A: 1\n"
        ":B+: 2 3\n"
        ":C:\n"
        ":D:   spaced  \n"
        ":END:\n"
        "x\n";
    const std::string_view expected =
        "org-data 0 59 0 59 0\n"
        "  headline 0 59 4 59 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=H\n"
        "    section 4 59 4 59 0\n"
        "      property-drawer 4 57 17 51 0\n"
        "        node-property 17 23 - - 0 key=A value=1\n"
        "        node-property 23 32 - - 0 key=B+ value=2 3\n"
        "        node-property 32 36 - - 0 key=C value=\n"
        "        node-property 36 51 - - 0 key=D value=spaced\n"
        "      paragraph 57 59 57 59 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a blank line after the heading makes it a plain drawer",
    "[org][drawer]") {
    const std::string_view source =
        "* H\n"
        "\n"
        ":PROPERTIES:\n"
        ":A: 1\n"
        ":END:\n";
    const std::string_view expected =
        "org-data 0 30 0 30 0\n"
        "  headline 0 30 5 30 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=H\n"
        "    section 5 30 5 30 0\n"
        "      drawer 5 30 18 24 0\n"
        "        paragraph 18 24 18 24 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a property drawer may follow the planning line", "[org][drawer]") {
    const std::string_view source =
        "* H\n"
        "SCHEDULED: <2024-01-01 Mon>\n"
        ":PROPERTIES:\n"
        ":A: 1\n"
        ":END:\n";
    const std::string_view expected =
        "org-data 0 57 0 57 0\n"
        "  headline 0 57 4 57 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=H\n"
        "    section 4 57 4 57 0\n"
        "      planning 4 32 - - 0\n"
        "        timestamp 15 31 - - 0\n"
        "      property-drawer 32 57 45 51 0\n"
        "        node-property 45 51 - - 0 key=A value=1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("the drawer and its keys match in any case", "[org][drawer]") {
    const std::string_view source =
        "* H\n"
        ":properties:\n"
        ":a: 1\n"
        ":end:\n";
    const std::string_view expected =
        "org-data 0 29 0 29 0\n"
        "  headline 0 29 4 29 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=H\n"
        "    section 4 29 4 29 0\n"
        "      property-drawer 4 29 17 23 0\n"
        "        node-property 17 23 - - 0 key=A value=1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a property drawer that is not closed is a paragraph",
    "[org][drawer]") {
    const std::string_view source =
        "* H\n"
        ":PROPERTIES:\n"
        ":A: 1\n";
    const std::string_view expected =
        "org-data 0 23 0 23 0\n"
        "  headline 0 23 4 23 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=H\n"
        "    section 4 23 4 23 0\n"
        "      paragraph 4 23 4 23 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a line that is no property makes it a plain drawer",
    "[org][drawer]") {
    const std::string_view source =
        "* H\n"
        ":PROPERTIES:\n"
        "foo\n"
        ":END:\n";
    const std::string_view expected =
        "org-data 0 27 0 27 0\n"
        "  headline 0 27 4 27 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=H\n"
        "    section 4 27 4 27 0\n"
        "      drawer 4 27 17 21 0\n"
        "        paragraph 17 21 17 21 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an empty property drawer has no contents", "[org][drawer]") {
    const std::string_view source =
        "* H\n"
        ":PROPERTIES:\n"
        ":END:\n";
    const std::string_view expected =
        "org-data 0 23 0 23 0\n"
        "  headline 0 23 4 23 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=H\n"
        "    section 4 23 4 23 0\n"
        "      property-drawer 4 23 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a property drawer may be indented", "[org][drawer]") {
    const std::string_view source =
        "* H\n"
        "  :PROPERTIES:\n"
        "  :A: 1\n"
        "  :END:\n";
    const std::string_view expected =
        "org-data 0 35 0 35 0\n"
        "  headline 0 35 4 35 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=H\n"
        "    section 4 35 4 35 0\n"
        "      property-drawer 4 35 19 27 0\n"
        "        node-property 19 27 - - 0 key=A value=1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a drawer holds elements", "[org][drawer]") {
    const std::string_view source =
        ":LOGBOOK:\n"
        "x\n"
        "\n"
        "- i\n"
        ":END:\n"
        "\n"
        "after\n";
    const std::string_view expected =
        "org-data 0 30 0 30 0\n"
        "  section 0 30 0 30 0\n"
        "    drawer 0 24 10 17 1\n"
        "      paragraph 10 13 10 12 1\n"
        "      plain-list 13 17 13 17 0 type=unordered\n"
        "        item 13 17 15 17 0 checkbox=-\n"
        "          paragraph 15 17 15 17 0\n"
        "    paragraph 24 30 24 30 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a drawer that is not closed is a paragraph", "[org][drawer]") {
    const std::string_view source =
        ":LOG:\n"
        "x\n";
    const std::string_view expected =
        "org-data 0 8 0 8 0\n"
        "  section 0 8 0 8 0\n"
        "    paragraph 0 8 0 8 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an empty drawer has no contents", "[org][drawer]") {
    const std::string_view source =
        ":D:\n"
        ":END:\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    drawer 0 10 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a closed drawer ends a paragraph", "[org][drawer]") {
    const std::string_view source =
        "text\n"
        ":D:\n"
        "x\n"
        ":END:\n"
        "more\n";
    const std::string_view expected =
        "org-data 0 22 0 22 0\n"
        "  section 0 22 0 22 0\n"
        "    paragraph 0 5 0 5 0\n"
        "    drawer 5 17 9 11 0\n"
        "      paragraph 9 11 9 11 0\n"
        "    paragraph 17 22 17 22 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an open drawer line does not end a paragraph", "[org][drawer]") {
    const std::string_view source =
        "text\n"
        ":D:\n"
        "x\n";
    const std::string_view expected =
        "org-data 0 11 0 11 0\n"
        "  section 0 11 0 11 0\n"
        "    paragraph 0 11 0 11 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a stray end line is text", "[org][drawer]") {
    const std::string_view source =
        ":END:\n";
    const std::string_view expected =
        "org-data 0 6 0 6 0\n"
        "  section 0 6 0 6 0\n"
        "    paragraph 0 6 0 6 0\n";
    CHECK(dump_of(source) == expected);
}

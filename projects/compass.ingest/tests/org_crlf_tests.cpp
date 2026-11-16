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

TEST_CASE("CRLF line ends parse like LF", "[org][crlf]") {
    const std::string_view source =
        "* H\r\n"
        ":PROPERTIES:\r\n"
        ":ID: x\r\n"
        ":END:\r\n"
        "\r\n"
        "para *b*\r\n"
        "line\r\n"
        "\r\n"
        "- a\r\n"
        "- b\r\n"
        "\r\n"
        "| a | b |\r\n"
        "#+begin_src c\r\n"
        "x\r\n"
        "#+end_src\r\n";
    const std::string_view expected =
        "org-data 0 106 0 106 0\n"
        "  headline 0 106 5 106 0 level=1 todo=- todo-type=- priority=- commen"
        "ted=0 tags=- title=H\n"
        "    section 5 106 5 106 0\n"
        "      property-drawer 5 36 19 27 1\n"
        "        node-property 19 27 - - 0 key=ID value=x\n"
        "      paragraph 36 54 36 52 1\n"
        "        bold 41 44 42 43 0\n"
        "      plain-list 54 66 54 64 1 type=unordered\n"
        "        item 54 59 56 59 0 checkbox=-\n"
        "          paragraph 56 59 56 59 0\n"
        "        item 59 64 61 64 0 checkbox=-\n"
        "          paragraph 61 64 61 64 0\n"
        "      table 66 77 66 77 0 type=org\n"
        "        table-row 66 77 67 75 0 type=standard\n"
        "          table-cell 67 71 68 69 0\n"
        "          table-cell 71 75 72 73 0\n"
        "      src-block 77 106 - - 0 name=c parameters=\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a file that starts with LF keeps later carriage returns as text",
    "[org][crlf]") {
    const std::string_view source =
        "a\n"
        "b\r\n"
        "\r\n"
        "c\r\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    paragraph 0 10 0 10 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("one bare LF makes every carriage return text", "[org][crlf]") {
    const std::string_view source =
        "a\r\n"
        "b\r\n"
        "c\r\n"
        "d\n"
        "e\r\n"
        "\r\n"
        "f\r\n";
    const std::string_view expected =
        "org-data 0 19 0 19 0\n"
        "  section 0 19 0 19 0\n"
        "    paragraph 0 19 0 19 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("CRLF blank lines count as post-blank", "[org][crlf]") {
    const std::string_view source =
        "a\r\n"
        "\r\n"
        "\r\n"
        "b\r\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    paragraph 0 7 0 3 2\n"
        "    paragraph 7 10 7 10 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a CRLF heading with tags", "[org][crlf]") {
    const std::string_view source =
        "* TODO t :x:\r\n";
    const std::string_view expected =
        "org-data 0 14 0 14 0\n"
        "  headline 0 14 - - 0 level=1 todo=TODO todo-type=todo priority=- com"
        "mented=0 tags=x title=t\n";
    CHECK(dump_of(source) == expected);
}

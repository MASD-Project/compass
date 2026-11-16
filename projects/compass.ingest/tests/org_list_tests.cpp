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

TEST_CASE("items of one list", "[org][list]") {
    const std::string_view source =
        "- a\n"
        "- b\n";
    const std::string_view expected =
        "org-data 0 8 0 8 0\n"
        "  section 0 8 0 8 0\n"
        "    plain-list 0 8 0 8 0 type=unordered\n"
        "      item 0 4 2 4 0 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "      item 4 8 6 8 0 checkbox=-\n"
        "        paragraph 6 8 6 8 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("lists nest by indentation", "[org][list]") {
    const std::string_view source =
        "- a\n"
        "  - b\n"
        "    - c\n"
        "  - d\n"
        "- e\n";
    const std::string_view expected =
        "org-data 0 28 0 28 0\n"
        "  section 0 28 0 28 0\n"
        "    plain-list 0 28 0 28 0 type=unordered\n"
        "      item 0 24 2 24 0 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "        plain-list 4 24 4 24 0 type=unordered\n"
        "          item 4 18 8 18 0 checkbox=-\n"
        "            paragraph 8 10 8 10 0\n"
        "            plain-list 10 18 10 18 0 type=unordered\n"
        "              item 10 18 16 18 0 checkbox=-\n"
        "                paragraph 16 18 16 18 0\n"
        "          item 18 24 22 24 0 checkbox=-\n"
        "            paragraph 22 24 22 24 0\n"
        "      item 24 28 26 28 0 checkbox=-\n"
        "        paragraph 26 28 26 28 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("ordered lists take a dot or a parenthesis", "[org][list]") {
    const std::string_view source =
        "1. a\n"
        "2) b\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    plain-list 0 10 0 10 0 type=ordered\n"
        "      item 0 5 3 5 0 checkbox=-\n"
        "        paragraph 3 5 3 5 0\n"
        "      item 5 10 8 10 0 checkbox=-\n"
        "        paragraph 8 10 8 10 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a letter bullet is text", "[org][list]") {
    const std::string_view source =
        "a. x\n"
        "b. y\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    paragraph 0 10 0 10 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a descriptive list has tags", "[org][list]") {
    const std::string_view source =
        "- t :: d\n"
        "- u ::\n"
        "- v :: w :: x\n"
        "- *b* =v= :: y\n";
    const std::string_view expected =
        "org-data 0 45 0 45 0\n"
        "  section 0 45 0 45 0\n"
        "    plain-list 0 45 0 45 0 type=descriptive\n"
        "      item 0 9 7 9 0 checkbox=-\n"
        "        paragraph 7 9 7 9 0\n"
        "      item 9 16 - - 1 checkbox=-\n"
        "      item 16 30 28 30 0 checkbox=-\n"
        "        paragraph 28 30 28 30 0\n"
        "      item 30 45 43 45 0 checkbox=-\n"
        "        bold 32 36 33 34 1\n"
        "        verbatim 36 39 - - 0\n"
        "        paragraph 43 45 43 45 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an ordered item keeps a double colon as text", "[org][list]") {
    const std::string_view source =
        "1. t :: d\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    plain-list 0 10 0 10 0 type=ordered\n"
        "      item 0 10 3 10 0 checkbox=-\n"
        "        paragraph 3 10 3 10 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("checkboxes", "[org][list]") {
    const std::string_view source =
        "- [ ] a\n"
        "- [X] b\n"
        "- [-] c\n"
        "- [x] d\n"
        "- [] e\n"
        "- [ ]f\n";
    const std::string_view expected =
        "org-data 0 46 0 46 0\n"
        "  section 0 46 0 46 0\n"
        "    plain-list 0 46 0 46 0 type=unordered\n"
        "      item 0 8 6 8 0 checkbox=off\n"
        "        paragraph 6 8 6 8 0\n"
        "      item 8 16 14 16 0 checkbox=on\n"
        "        paragraph 14 16 14 16 0\n"
        "      item 16 24 22 24 0 checkbox=trans\n"
        "        paragraph 22 24 22 24 0\n"
        "      item 24 32 30 32 0 checkbox=-\n"
        "        paragraph 30 32 30 32 0\n"
        "      item 32 39 34 39 0 checkbox=-\n"
        "        paragraph 34 39 34 39 0\n"
        "      item 39 46 41 46 0 checkbox=-\n"
        "        paragraph 41 46 41 46 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a counter cookie precedes the checkbox", "[org][list]") {
    const std::string_view source =
        "- [@3] a\n"
        "1. [@start:5] [X] b\n";
    const std::string_view expected =
        "org-data 0 29 0 29 0\n"
        "  section 0 29 0 29 0\n"
        "    plain-list 0 29 0 29 0 type=unordered\n"
        "      item 0 9 7 9 0 checkbox=-\n"
        "        paragraph 7 9 7 9 0\n"
        "      item 9 29 27 29 0 checkbox=on\n"
        "        paragraph 27 29 27 29 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a star is a bullet only when indented", "[org][list]") {
    const std::string_view source =
        " * a\n"
        " * b\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    plain-list 0 10 0 10 0 type=unordered\n"
        "      item 0 5 3 5 0 checkbox=-\n"
        "        paragraph 3 5 3 5 0\n"
        "      item 5 10 8 10 0 checkbox=-\n"
        "        paragraph 8 10 8 10 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("two blank lines end a list", "[org][list]") {
    const std::string_view source =
        "- a\n"
        "\n"
        "\n"
        "- b\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    plain-list 0 6 0 4 2 type=unordered\n"
        "      item 0 4 2 4 0 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "    plain-list 6 10 6 10 0 type=unordered\n"
        "      item 6 10 8 10 0 checkbox=-\n"
        "        paragraph 8 10 8 10 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("one blank line stays inside the list", "[org][list]") {
    const std::string_view source =
        "- a\n"
        "\n"
        "- b\n";
    const std::string_view expected =
        "org-data 0 9 0 9 0\n"
        "  section 0 9 0 9 0\n"
        "    plain-list 0 9 0 9 0 type=unordered\n"
        "      item 0 5 2 4 1 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "      item 5 9 7 9 0 checkbox=-\n"
        "        paragraph 7 9 7 9 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an item holds paragraphs indented under it", "[org][list]") {
    const std::string_view source =
        "- a\n"
        "  cont\n"
        "\n"
        "  second\n"
        "- b\n";
    const std::string_view expected =
        "org-data 0 25 0 25 0\n"
        "  section 0 25 0 25 0\n"
        "    plain-list 0 25 0 25 0 type=unordered\n"
        "      item 0 21 2 21 0 checkbox=-\n"
        "        paragraph 2 12 2 11 1\n"
        "        paragraph 12 21 12 21 0\n"
        "      item 21 25 23 25 0 checkbox=-\n"
        "        paragraph 23 25 23 25 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("text at the bullet column ends the list", "[org][list]") {
    const std::string_view source =
        "- a\n"
        "text\n";
    const std::string_view expected =
        "org-data 0 9 0 9 0\n"
        "  section 0 9 0 9 0\n"
        "    plain-list 0 4 0 4 0 type=unordered\n"
        "      item 0 4 2 4 0 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "    paragraph 4 9 4 9 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a block inside an item hides its lines from the list",
    "[org][list]") {
    const std::string_view source =
        "- a\n"
        "  #+begin_src c\n"
        "  - not item\n"
        "  #+end_src\n"
        "- b\n";
    const std::string_view expected =
        "org-data 0 49 0 49 0\n"
        "  section 0 49 0 49 0\n"
        "    plain-list 0 49 0 49 0 type=unordered\n"
        "      item 0 45 2 45 0 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "        src-block 4 45 - - 0 name=c parameters=\n"
        "      item 45 49 47 49 0 checkbox=-\n"
        "        paragraph 47 49 47 49 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a block at the bullet column ends the list", "[org][list]") {
    const std::string_view source =
        "- a\n"
        "#+begin_src c\n"
        "- x\n"
        "#+end_src\n";
    const std::string_view expected =
        "org-data 0 32 0 32 0\n"
        "  section 0 32 0 32 0\n"
        "    plain-list 0 4 0 4 0 type=unordered\n"
        "      item 0 4 2 4 0 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "    src-block 4 32 - - 0 name=c parameters=\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an empty item has no contents", "[org][list]") {
    const std::string_view source =
        "- \n"
        "- b\n"
        "-\n";
    const std::string_view expected =
        "org-data 0 9 0 9 0\n"
        "  section 0 9 0 9 0\n"
        "    plain-list 0 9 0 9 0 type=unordered\n"
        "      item 0 3 - - 1 checkbox=-\n"
        "      item 3 7 5 7 0 checkbox=-\n"
        "        paragraph 5 7 5 7 0\n"
        "      item 7 9 - - 1 checkbox=-\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a list inside a quote block inside an item", "[org][list]") {
    const std::string_view source =
        "- a\n"
        "  #+begin_quote\n"
        "  - q\n"
        "  #+end_quote\n"
        "- b\n";
    const std::string_view expected =
        "org-data 0 44 0 44 0\n"
        "  section 0 44 0 44 0\n"
        "    plain-list 0 44 0 44 0 type=unordered\n"
        "      item 0 40 2 40 0 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "        quote-block 4 40 20 26 0\n"
        "          plain-list 20 26 20 26 0 type=unordered\n"
        "            item 20 26 24 26 0 checkbox=-\n"
        "              paragraph 24 26 24 26 0\n"
        "      item 40 44 42 44 0 checkbox=-\n"
        "        paragraph 42 44 42 44 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an item at the end of a file without a line feed", "[org][list]") {
    const std::string_view source =
        "- a";
    const std::string_view expected =
        "org-data 0 3 0 3 0\n"
        "  section 0 3 0 3 0\n"
        "    plain-list 0 3 0 3 0 type=unordered\n"
        "      item 0 3 2 3 0 checkbox=-\n"
        "        paragraph 2 3 2 3 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("tabs indent to the next multiple of eight", "[org][list]") {
    const std::string_view source =
        "\t- a\n"
        "        - b\n"
        "\t  - c\n";
    const std::string_view expected =
        "org-data 0 24 0 24 0\n"
        "  section 0 24 0 24 0\n"
        "    plain-list 0 24 0 24 0 type=unordered\n"
        "      item 0 5 3 5 0 checkbox=-\n"
        "        paragraph 3 5 3 5 0\n"
        "      item 5 24 15 24 0 checkbox=-\n"
        "        paragraph 15 17 15 17 0\n"
        "        plain-list 17 24 17 24 0 type=unordered\n"
        "          item 17 24 22 24 0 checkbox=-\n"
        "            paragraph 22 24 22 24 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a list and a table inside an item", "[org][list]") {
    const std::string_view source =
        "- a\n"
        "  | t |\n"
        "  1. n\n"
        "- b\n";
    const std::string_view expected =
        "org-data 0 23 0 23 0\n"
        "  section 0 23 0 23 0\n"
        "    plain-list 0 23 0 23 0 type=unordered\n"
        "      item 0 19 2 19 0 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "        table 4 12 4 12 0 type=org\n"
        "          table-row 4 12 7 11 0 type=standard\n"
        "            table-cell 7 11 8 9 0\n"
        "        plain-list 12 19 12 19 0 type=ordered\n"
        "          item 12 19 17 19 0 checkbox=-\n"
        "            paragraph 17 19 17 19 0\n"
        "      item 19 23 21 23 0 checkbox=-\n"
        "        paragraph 21 23 21 23 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a drawer inside an item", "[org][list]") {
    const std::string_view source =
        "- a\n"
        "  :D:\n"
        "  - x\n"
        "  :END:\n"
        "- b\n";
    const std::string_view expected =
        "org-data 0 28 0 28 0\n"
        "  section 0 28 0 28 0\n"
        "    plain-list 0 28 0 28 0 type=unordered\n"
        "      item 0 24 2 24 0 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "        drawer 4 24 10 16 0\n"
        "          plain-list 10 16 10 16 0 type=unordered\n"
        "            item 10 16 14 16 0 checkbox=-\n"
        "              paragraph 14 16 14 16 0\n"
        "      item 24 28 26 28 0 checkbox=-\n"
        "        paragraph 26 28 26 28 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a sublist of another type", "[org][list]") {
    const std::string_view source =
        "- a\n"
        "  1. b\n"
        "  2. c\n"
        "- d\n";
    const std::string_view expected =
        "org-data 0 22 0 22 0\n"
        "  section 0 22 0 22 0\n"
        "    plain-list 0 22 0 22 0 type=unordered\n"
        "      item 0 18 2 18 0 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "        plain-list 4 18 4 18 0 type=ordered\n"
        "          item 4 11 9 11 0 checkbox=-\n"
        "            paragraph 9 11 9 11 0\n"
        "          item 11 18 16 18 0 checkbox=-\n"
        "            paragraph 16 18 16 18 0\n"
        "      item 18 22 20 22 0 checkbox=-\n"
        "        paragraph 20 22 20 22 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("blank lines between items are the post-blank of the item",
    "[org][list]") {
    const std::string_view source =
        "- a\n"
        "\n"
        "- b\n"
        "\n"
        "\n"
        "x\n";
    const std::string_view expected =
        "org-data 0 13 0 13 0\n"
        "  section 0 13 0 13 0\n"
        "    plain-list 0 11 0 9 2 type=unordered\n"
        "      item 0 5 2 4 1 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "      item 5 9 7 9 0 checkbox=-\n"
        "        paragraph 7 9 7 9 0\n"
        "    paragraph 11 13 11 13 0\n";
    CHECK(dump_of(source) == expected);
}

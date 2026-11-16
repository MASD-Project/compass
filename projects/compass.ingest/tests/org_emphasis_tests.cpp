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

TEST_CASE("the six markers", "[org][emphasis]") {
    const std::string_view source =
        "*b* /i/ _u_ +s+ ~c~ =v=\n";
    const std::string_view expected =
        "org-data 0 24 0 24 0\n"
        "  section 0 24 0 24 0\n"
        "    paragraph 0 24 0 24 0\n"
        "      bold 0 4 1 2 1\n"
        "      italic 4 8 5 6 1\n"
        "      underline 8 12 9 10 1\n"
        "      strike-through 12 16 13 14 1\n"
        "      code 16 20 - - 1\n"
        "      verbatim 20 23 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("markup needs a border before it", "[org][emphasis]") {
    const std::string_view source =
        "a*b*\n";
    const std::string_view expected =
        "org-data 0 5 0 5 0\n"
        "  section 0 5 0 5 0\n"
        "    paragraph 0 5 0 5 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("markup needs a border after it", "[org][emphasis]") {
    const std::string_view source =
        "*b*c\n";
    const std::string_view expected =
        "org-data 0 5 0 5 0\n"
        "  section 0 5 0 5 0\n"
        "    paragraph 0 5 0 5 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("markup may not start or end with a space", "[org][emphasis]") {
    const std::string_view source =
        "x * b* *b * y\n";
    const std::string_view expected =
        "org-data 0 14 0 14 0\n"
        "  section 0 14 0 14 0\n"
        "    paragraph 0 14 0 14 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("characters that may precede markup", "[org][emphasis]") {
    const std::string_view source =
        "(*b*) \"*b*\" {*b*} -*b*- '*b*'\n";
    const std::string_view expected =
        "org-data 0 30 0 30 0\n"
        "  section 0 30 0 30 0\n"
        "    paragraph 0 30 0 30 0\n"
        "      bold 1 4 2 3 0\n"
        "      bold 7 10 8 9 0\n"
        "      bold 13 16 14 15 0\n"
        "      bold 19 22 20 21 0\n"
        "      bold 25 28 26 27 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("characters that may follow markup", "[org][emphasis]") {
    const std::string_view source =
        "*b*. *b*, *b*; *b*: *b*! *b*\? *b*) *b*} *b*[ *b*\\\n";
    const std::string_view expected =
        "org-data 0 50 0 50 0\n"
        "  section 0 50 0 50 0\n"
        "    paragraph 0 50 0 50 0\n"
        "      bold 0 3 1 2 0\n"
        "      bold 5 8 6 7 0\n"
        "      bold 10 13 11 12 0\n"
        "      bold 15 18 16 17 0\n"
        "      bold 20 23 21 22 0\n"
        "      bold 25 28 26 27 0\n"
        "      bold 30 33 31 32 0\n"
        "      bold 35 38 36 37 0\n"
        "      bold 40 43 41 42 0\n"
        "      bold 45 48 46 47 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("empty and doubled markers", "[org][emphasis]") {
    const std::string_view source =
        "** *a**b* ****\n";
    const std::string_view expected =
        "org-data 0 15 0 15 0\n"
        "  headline 0 15 - - 0 level=2 todo=- todo-type=- priority=- commented"
        "=0 tags=- title=*a**b* ****\n"
        "    bold 3 10 4 8 1\n"
        "    bold 10 14 11 13 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("markup spans lines but not paragraphs", "[org][emphasis]") {
    const std::string_view source =
        "*a\n"
        "b* c\n"
        "\n"
        "*d\n"
        "\n"
        "e*\n";
    const std::string_view expected =
        "org-data 0 16 0 16 0\n"
        "  section 0 16 0 16 0\n"
        "    paragraph 0 9 0 8 1\n"
        "      bold 0 6 1 4 1\n"
        "    paragraph 9 13 9 12 1\n"
        "    paragraph 13 16 13 16 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("markup nests", "[org][emphasis]") {
    const std::string_view source =
        "*b /i _u_ i/ b*\n";
    const std::string_view expected =
        "org-data 0 16 0 16 0\n"
        "  section 0 16 0 16 0\n"
        "    paragraph 0 16 0 16 0\n"
        "      bold 0 15 1 14 0\n"
        "        italic 3 13 4 11 1\n"
        "          underline 6 10 7 8 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("verbatim and code hide markup", "[org][emphasis]") {
    const std::string_view source =
        "=*a* /b/= ~[[x]]~ =a_b=\n";
    const std::string_view expected =
        "org-data 0 24 0 24 0\n"
        "  section 0 24 0 24 0\n"
        "    paragraph 0 24 0 24 0\n"
        "      verbatim 0 10 - - 1\n"
        "      code 10 18 - - 1\n"
        "      verbatim 18 23 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("verbatim closes at the first marker with a border after it",
    "[org][emphasis]") {
    const std::string_view source =
        "=a= b= =a=b= c\n";
    const std::string_view expected =
        "org-data 0 15 0 15 0\n"
        "  section 0 15 0 15 0\n"
        "    paragraph 0 15 0 15 0\n"
        "      verbatim 0 4 - - 1\n"
        "      verbatim 7 13 - - 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("letters above ASCII are not borders but dashes are",
    "[org][emphasis]") {
    const std::string_view source =
        "\303\251*b* *b*\303\251 \342\200\224*b*\342\200\224\n";
    const std::string_view expected =
        "org-data 0 22 0 22 0\n"
        "  section 0 22 0 22 0\n"
        "    paragraph 0 22 0 22 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a zero-width space is a border", "[org][emphasis]") {
    const std::string_view source =
        "=v=\342\200\213s\n";
    const std::string_view expected =
        "org-data 0 8 0 8 0\n"
        "  section 0 8 0 8 0\n"
        "    paragraph 0 8 0 8 0\n"
        "      verbatim 0 3 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("equals signs in prose", "[org][emphasis]") {
    const std::string_view source =
        "x=1, y=2\n"
        "a = b = c\n";
    const std::string_view expected =
        "org-data 0 19 0 19 0\n"
        "  section 0 19 0 19 0\n"
        "    paragraph 0 19 0 19 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("tildes and slashes in paths", "[org][emphasis]") {
    const std::string_view source =
        "~/.config and ~/bin\n"
        "/usr/bin/ and /etc/\n";
    const std::string_view expected =
        "org-data 0 40 0 40 0\n"
        "  section 0 40 0 40 0\n"
        "    paragraph 0 40 0 40 0\n"
        "      italic 20 30 21 28 1\n"
        "      italic 34 39 35 38 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("plus and star in arithmetic", "[org][emphasis]") {
    const std::string_view source =
        "+1 +2+ 2*3*4\n";
    const std::string_view expected =
        "org-data 0 13 0 13 0\n"
        "  section 0 13 0 13 0\n"
        "    paragraph 0 13 0 13 0\n"
        "      strike-through 0 7 1 5 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an underscore word is a subscript, not an underline",
    "[org][emphasis]") {
    const std::string_view source =
        "a_b_c _u_ x_y\n";
    const std::string_view expected =
        "org-data 0 14 0 14 0\n"
        "  section 0 14 0 14 0\n"
        "    paragraph 0 14 0 14 0\n"
        "      subscript 1 3 2 3 0\n"
        "      subscript 3 6 4 5 1\n"
        "      underline 6 10 7 8 1\n"
        "      subscript 11 13 12 13 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("subscripts and superscripts", "[org][emphasis]") {
    const std::string_view source =
        "a_{sub} a_1 a_b.c, a^2 a^{x y} a^(x) a^* a_-1 a_+ x^\\alpha\n";
    const std::string_view expected =
        "org-data 0 59 0 59 0\n"
        "  section 0 59 0 59 0\n"
        "    paragraph 0 59 0 59 0\n"
        "      subscript 1 8 3 6 1\n"
        "      subscript 9 12 10 11 1\n"
        "      subscript 13 17 14 17 0\n"
        "      superscript 20 23 21 22 1\n"
        "      superscript 24 31 26 29 1\n"
        "      superscript 32 37 33 36 1\n"
        "      superscript 38 41 39 40 1\n"
        "      subscript 42 46 43 45 1\n"
        "      entity 52 58 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("no script at the start of a line or after a space",
    "[org][emphasis]") {
    const std::string_view source =
        "_a\n"
        " ^a x _b\n";
    const std::string_view expected =
        "org-data 0 12 0 12 0\n"
        "  section 0 12 0 12 0\n"
        "    paragraph 0 12 0 12 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("braces of a script nest three deep", "[org][emphasis]") {
    const std::string_view source =
        "a_{b{c}d} a_{b{c{d}e}f} a_{b{c{d{e}}}}\n";
    const std::string_view expected =
        "org-data 0 39 0 39 0\n"
        "  section 0 39 0 39 0\n"
        "    paragraph 0 39 0 39 0\n"
        "      subscript 1 10 3 8 1\n"
        "      subscript 11 24 13 22 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a caret before a backslash is no superscript", "[org][emphasis]") {
    const std::string_view source =
        "\"^\\s+\"\n";
    const std::string_view expected =
        "org-data 0 7 0 7 0\n"
        "  section 0 7 0 7 0\n"
        "    paragraph 0 7 0 7 0\n"
        "      latex-fragment 2 4 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("scripts hold objects", "[org][emphasis]") {
    const std::string_view source =
        "a_{*b*} a^{=v=}\n";
    const std::string_view expected =
        "org-data 0 16 0 16 0\n"
        "  section 0 16 0 16 0\n"
        "    paragraph 0 16 0 16 0\n"
        "      subscript 1 8 3 6 1\n"
        "        bold 3 6 4 5 0\n"
        "      superscript 9 15 11 14 0\n"
        "        verbatim 11 14 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("markup holds links and a link description holds markup",
    "[org][emphasis]") {
    const std::string_view source =
        "*a [[id:x][b /i/]] c* /[[id:y]]/\n";
    const std::string_view expected =
        "org-data 0 33 0 33 0\n"
        "  section 0 33 0 33 0\n"
        "    paragraph 0 33 0 33 0\n"
        "      bold 0 22 1 20 1\n"
        "        link 3 19 11 16 1 type=id format=bracket path=x\n"
        "          italic 13 16 14 15 0\n"
        "      italic 22 32 23 31 0\n"
        "        link 23 31 - - 0 type=id format=bracket path=y\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a script after a letter above ASCII", "[org][emphasis]") {
    const std::string_view source =
        "\303\251_b x_\303\251\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    paragraph 0 10 0 10 0\n"
        "      subscript 2 5 3 4 1\n"
        "      subscript 6 9 7 9 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("markup inside a footnote, a cell and an item tag",
    "[org][emphasis]") {
    const std::string_view source =
        "[fn:1] *b*\n"
        "\n"
        "| /i/ |\n"
        "\n"
        "- =v= :: d\n";
    const std::string_view expected =
        "org-data 0 32 0 32 0\n"
        "  section 0 32 0 32 0\n"
        "    footnote-definition 0 32 7 32 0\n"
        "      paragraph 7 12 7 11 1\n"
        "        bold 7 10 8 9 0\n"
        "      table 12 21 12 20 1 type=org\n"
        "        table-row 12 20 13 19 0 type=standard\n"
        "          table-cell 13 19 14 17 0\n"
        "            italic 14 17 15 16 0\n"
        "      plain-list 21 32 21 32 0 type=descriptive\n"
        "        item 21 32 30 32 0 checkbox=-\n"
        "          verbatim 23 26 - - 0\n"
        "          paragraph 30 32 30 32 0\n";
    CHECK(dump_of(source) == expected);
}

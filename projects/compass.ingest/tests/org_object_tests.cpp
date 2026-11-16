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

TEST_CASE("entities", "[org][object]") {
    const std::string_view source =
        "\\alpha \\alpha{} \\alphax \\nbsp{}x \\there4 \\sup2 \\frac12 \\Agrav"
        "e \\_ x\n";
    const std::string_view expected =
        "org-data 0 68 0 68 0\n"
        "  section 0 68 0 68 0\n"
        "    paragraph 0 68 0 68 0\n"
        "      entity 0 7 - - 1\n"
        "      entity 7 16 - - 1\n"
        "      latex-fragment 16 24 - - 1\n"
        "      entity 24 31 - - 0\n"
        "      entity 33 41 - - 1\n"
        "      entity 41 47 - - 1\n"
        "      entity 47 55 - - 1\n"
        "      entity 55 63 - - 1\n"
        "      entity 63 66 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an unknown command is a LaTeX fragment", "[org][object]") {
    const std::string_view source =
        "\\foo \\cmd{a}[b]{c} \\cmd* C:\\Users\\me\n";
    const std::string_view expected =
        "org-data 0 37 0 37 0\n"
        "  section 0 37 0 37 0\n"
        "    paragraph 0 37 0 37 0\n"
        "      latex-fragment 0 5 - - 1\n"
        "      latex-fragment 5 19 - - 1\n"
        "      latex-fragment 19 25 - - 1\n"
        "      latex-fragment 27 33 - - 0\n"
        "      latex-fragment 33 36 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("LaTeX fragments in dollars and brackets", "[org][object]") {
    const std::string_view source =
        "$x$ $$y$$ \\(z\\) \\[w\\] $a+b$.\n";
    const std::string_view expected =
        "org-data 0 29 0 29 0\n"
        "  section 0 29 0 29 0\n"
        "    paragraph 0 29 0 29 0\n"
        "      latex-fragment 0 4 - - 1\n"
        "      latex-fragment 4 10 - - 1\n"
        "      latex-fragment 10 16 - - 1\n"
        "      latex-fragment 16 22 - - 1\n"
        "      latex-fragment 22 27 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("what is not a dollar fragment", "[org][object]") {
    const std::string_view source =
        "$ a$ $a $ a$b$c $1,000 and $2 x$$\n";
    const std::string_view expected =
        "org-data 0 34 0 34 0\n"
        "  section 0 34 0 34 0\n"
        "    paragraph 0 34 0 34 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("timestamps", "[org][object]") {
    const std::string_view source =
        "<2024-01-01 Mon> [2024-01-01 Mon 10:00] <2024-01-01 Mon>--<2024-01-02"
        " Tue> <2024-01-01 Mon +1w> [2024-1-1] <2024-01-01>\n";
    const std::string_view expected =
        "org-data 0 120 0 120 0\n"
        "  section 0 120 0 120 0\n"
        "    paragraph 0 120 0 120 0\n"
        "      timestamp 0 17 - - 1\n"
        "      timestamp 17 40 - - 1\n"
        "      timestamp 40 75 - - 1\n"
        "      timestamp 75 96 - - 1\n"
        "      timestamp 107 119 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a diary timestamp", "[org][object]") {
    const std::string_view source =
        "<%%(diary-float t 4 2)> x\n";
    const std::string_view expected =
        "org-data 0 26 0 26 0\n"
        "  section 0 26 0 26 0\n"
        "    paragraph 0 26 0 26 0\n"
        "      timestamp 0 24 - - 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("footnote references", "[org][object]") {
    const std::string_view source =
        "a[fn:1] b[fn:n:inline *b*] c[fn::anon [x]] d[fn:] e[fn:a b] f[fn:x\n";
    const std::string_view expected =
        "org-data 0 67 0 67 0\n"
        "  section 0 67 0 67 0\n"
        "    paragraph 0 67 0 67 0\n"
        "      footnote-reference 1 8 - - 1\n"
        "      footnote-reference 9 27 15 25 1\n"
        "        bold 22 25 23 24 0\n"
        "      footnote-reference 28 43 33 41 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("statistics cookies", "[org][object]") {
    const std::string_view source =
        "[1/2] [50%] [/] [%] [1/] [a/b] [12]\n";
    const std::string_view expected =
        "org-data 0 36 0 36 0\n"
        "  section 0 36 0 36 0\n"
        "    paragraph 0 36 0 36 0\n"
        "      statistics-cookie 0 6 - - 1\n"
        "      statistics-cookie 6 12 - - 1\n"
        "      statistics-cookie 12 16 - - 1\n"
        "      statistics-cookie 16 20 - - 1\n"
        "      statistics-cookie 20 25 - - 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("targets and radio targets", "[org][object]") {
    const std::string_view source =
        "<<t>> <<<r *b*>>> << x>> <<a b>> <<a\n"
        "b>>\n";
    const std::string_view expected =
        "org-data 0 41 0 41 0\n"
        "  section 0 41 0 41 0\n"
        "    paragraph 0 41 0 41 0\n"
        "      target 0 6 - - 1\n"
        "      radio-target 6 18 9 14 1\n"
        "        bold 11 14 12 13 0\n"
        "      target 25 33 - - 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("macros", "[org][object]") {
    const std::string_view source =
        "{{{m}}} {{{m(a,b)}}} {{{1x}}} {{{m(a\n"
        "b)}}} {{{m\n";
    const std::string_view expected =
        "org-data 0 48 0 48 0\n"
        "  section 0 48 0 48 0\n"
        "    paragraph 0 48 0 48 0\n"
        "      macro 0 8 - - 1\n"
        "      macro 8 21 - - 1\n"
        "      macro 30 43 - - 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("export snippets", "[org][object]") {
    const std::string_view source =
        "@@html:<b>@@ @@x@@ @@html:unclosed\n";
    const std::string_view expected =
        "org-data 0 35 0 35 0\n"
        "  section 0 35 0 35 0\n"
        "    paragraph 0 35 0 35 0\n"
        "      export-snippet 0 13 - - 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("line breaks", "[org][object]") {
    const std::string_view source =
        "a\\\\\n"
        "b\\\\  \n"
        "c \\\\ d\n"
        "e\\\\\\\n"
        "f\n";
    const std::string_view expected =
        "org-data 0 24 0 24 0\n"
        "  section 0 24 0 24 0\n"
        "    paragraph 0 24 0 24 0\n"
        "      line-break 1 4 - - 0\n"
        "      line-break 5 10 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("inline source blocks and babel calls", "[org][object]") {
    const std::string_view source =
        "src_c{x} src_c[:a b]{y} src_ {z} call_f(1) call_f[h](2)[t] xsrc_c{q} "
        "src_c\n";
    const std::string_view expected =
        "org-data 0 75 0 75 0\n"
        "  section 0 75 0 75 0\n"
        "    paragraph 0 75 0 75 0\n"
        "      inline-src-block 0 9 - - 1\n"
        "      inline-src-block 9 24 - - 1\n"
        "      inline-babel-call 33 43 - - 1\n"
        "      inline-babel-call 43 59 - - 1\n"
        "      subscript 63 65 64 65 0\n"
        "      subscript 72 74 73 74 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("objects at the end of a paragraph keep no post-blank past it",
    "[org][object]") {
    const std::string_view source =
        "*b*  \n"
        "\n"
        "=v=\n";
    const std::string_view expected =
        "org-data 0 11 0 11 0\n"
        "  section 0 11 0 11 0\n"
        "    paragraph 0 7 0 6 1\n"
        "      bold 0 5 1 2 2\n"
        "    paragraph 7 11 7 11 0\n"
        "      verbatim 7 10 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a line break is not an object in a heading or a table cell",
    "[org][object]") {
    const std::string_view source =
        "* h \\\\\n"
        "| c \\\\ |\n";
    const std::string_view expected =
        "org-data 0 16 0 16 0\n"
        "  headline 0 16 7 16 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=h \\\\\\\\\n"
        "    section 7 16 7 16 0\n"
        "      table 7 16 7 16 0 type=org\n"
        "        table-row 7 16 8 15 0 type=standard\n"
        "          table-cell 8 15 9 13 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an inline footnote holds a link", "[org][object]") {
    const std::string_view source =
        "x[fn::see [[id:a][b]]] y\n";
    const std::string_view expected =
        "org-data 0 25 0 25 0\n"
        "  section 0 25 0 25 0\n"
        "    paragraph 0 25 0 25 0\n"
        "      footnote-reference 1 23 6 21 1\n"
        "        link 10 21 18 19 0 type=id format=bracket path=a\n";
    CHECK(dump_of(source) == expected);
}

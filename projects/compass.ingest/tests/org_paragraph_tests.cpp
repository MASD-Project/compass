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

TEST_CASE("a blank line separates paragraphs", "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "b\n"
        "\n"
        "c\n";
    const std::string_view expected =
        "org-data 0 7 0 7 0\n"
        "  section 0 7 0 7 0\n"
        "    paragraph 0 5 0 4 1\n"
        "    paragraph 5 7 5 7 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("blank lines with spaces separate paragraphs", "[org][paragraph]") {
    const std::string_view source =
        "a  \n"
        "   \n"
        "b\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    paragraph 0 8 0 4 1\n"
        "    paragraph 8 10 8 10 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a table ends a paragraph", "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "| t |\n"
        "b\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    table 2 8 2 8 0 type=org\n"
        "      table-row 2 8 3 7 0 type=standard\n"
        "        table-cell 3 7 4 5 0\n"
        "    paragraph 8 10 8 10 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an item ends a paragraph", "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "- i\n";
    const std::string_view expected =
        "org-data 0 6 0 6 0\n"
        "  section 0 6 0 6 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    plain-list 2 6 2 6 0 type=unordered\n"
        "      item 2 6 4 6 0 checkbox=-\n"
        "        paragraph 4 6 4 6 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a numbered line needs a space after the terminator to be an item",
    "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "1. x\n"
        "b\n"
        "1.x\n";
    const std::string_view expected =
        "org-data 0 13 0 13 0\n"
        "  section 0 13 0 13 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    plain-list 2 7 2 7 0 type=ordered\n"
        "      item 2 7 5 7 0 checkbox=-\n"
        "        paragraph 5 7 5 7 0\n"
        "    paragraph 7 13 7 13 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an indented star with a space is an item and ends a paragraph",
    "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        " * x\n";
    const std::string_view expected =
        "org-data 0 7 0 7 0\n"
        "  section 0 7 0 7 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    plain-list 2 7 2 7 0 type=unordered\n"
        "      item 2 7 5 7 0 checkbox=-\n"
        "        paragraph 5 7 5 7 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a plus followed by a space is an item", "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "+ x\n";
    const std::string_view expected =
        "org-data 0 6 0 6 0\n"
        "  section 0 6 0 6 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    plain-list 2 6 2 6 0 type=unordered\n"
        "      item 2 6 4 6 0 checkbox=-\n"
        "        paragraph 4 6 4 6 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a block that closes ends a paragraph", "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "#+begin_quote\n"
        "q\n"
        "#+end_quote\n";
    const std::string_view expected =
        "org-data 0 30 0 30 0\n"
        "  section 0 30 0 30 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    quote-block 2 30 16 18 0\n"
        "      paragraph 16 18 16 18 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a block that never closes is part of the paragraph",
    "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "#+begin_x\n"
        "b\n";
    const std::string_view expected =
        "org-data 0 14 0 14 0\n"
        "  section 0 14 0 14 0\n"
        "    paragraph 0 14 0 14 0\n"
        "      subscript 9 11 10 11 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a comment line ends a paragraph", "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "# c\n"
        "b\n";
    const std::string_view expected =
        "org-data 0 8 0 8 0\n"
        "  section 0 8 0 8 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    comment 2 6 - - 0\n"
        "    paragraph 6 8 6 8 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a fixed-width line ends a paragraph", "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        ": f\n"
        "b\n";
    const std::string_view expected =
        "org-data 0 8 0 8 0\n"
        "  section 0 8 0 8 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    fixed-width 2 6 - - 1\n"
        "    paragraph 6 8 6 8 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a rule ends a paragraph", "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "-----\n"
        "b\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    horizontal-rule 2 8 - - 0\n"
        "    paragraph 8 10 8 10 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a keyword ends a paragraph but a bare hash plus word does not",
    "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "#+key: v\n"
        "b\n"
        "#+key\n"
        "c\n";
    const std::string_view expected =
        "org-data 0 21 0 21 0\n"
        "  section 0 21 0 21 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    keyword 2 11 - - 0 key=key value=v\n"
        "    paragraph 11 21 11 21 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a LaTeX environment that closes ends a paragraph",
    "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "\\begin{eq}\n"
        "x\n"
        "\\end{eq}\n"
        "b\n";
    const std::string_view expected =
        "org-data 0 26 0 26 0\n"
        "  section 0 26 0 26 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    latex-environment 2 24 - - 0\n"
        "    paragraph 24 26 24 26 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a LaTeX environment that never closes is text",
    "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "\\begin{eq}\n"
        "b\n";
    const std::string_view expected =
        "org-data 0 15 0 15 0\n"
        "  section 0 15 0 15 0\n"
        "    paragraph 0 15 0 15 0\n"
        "      latex-fragment 2 12 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a footnote definition ends a paragraph", "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "[fn:1] d\n";
    const std::string_view expected =
        "org-data 0 11 0 11 0\n"
        "  section 0 11 0 11 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    footnote-definition 2 11 9 11 0\n"
        "      paragraph 9 11 9 11 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a diary sexp ends a paragraph", "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "%%(diary-float t 4 2)\n";
    const std::string_view expected =
        "org-data 0 24 0 24 0\n"
        "  section 0 24 0 24 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    diary-sexp 2 24 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a table.el border ends a paragraph", "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "+-----+\n";
    const std::string_view expected =
        "org-data 0 10 0 10 0\n"
        "  section 0 10 0 10 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    paragraph 2 10 2 10 0\n"
        "      strike-through 2 9 3 8 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a dual keyword with an option ends a paragraph, another key "
          "does not",
    "[org][paragraph]") {
    const std::string_view source =
        "a\n"
        "#+caption[s]: x\n"
        "| t |\n"
        "b\n"
        "#+foo[s]: x\n"
        "c\n";
    const std::string_view expected =
        "org-data 0 40 0 40 0\n"
        "  section 0 40 0 40 0\n"
        "    paragraph 0 2 0 2 0\n"
        "    table 2 24 18 24 0 type=org\n"
        "      affiliated key=caption\n"
        "      table-row 18 24 19 23 0 type=standard\n"
        "        table-cell 19 23 20 21 0\n"
        "    paragraph 24 40 24 40 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("comment lines form one comment", "[org][paragraph]") {
    const std::string_view source =
        "# a\n"
        "#\n"
        "# b\n"
        "\n"
        "#c\n";
    const std::string_view expected =
        "org-data 0 14 0 14 0\n"
        "  section 0 14 0 14 0\n"
        "    comment 0 11 - - 1\n"
        "    paragraph 11 14 11 14 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("fixed-width lines form one area with their own post-blank",
    "[org][paragraph]") {
    const std::string_view source =
        "  : a\n"
        ":\n"
        ": b\n"
        "\n"
        ":c\n";
    const std::string_view expected =
        "org-data 0 16 0 16 0\n"
        "  section 0 16 0 16 0\n"
        "    fixed-width 0 13 - - 2\n"
        "    paragraph 13 16 13 16 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a rule needs five dashes and nothing else", "[org][paragraph]") {
    const std::string_view source =
        "-----\n"
        "----\n"
        "------  \n"
        " ----- x\n";
    const std::string_view expected =
        "org-data 0 29 0 29 0\n"
        "  section 0 29 0 29 0\n"
        "    horizontal-rule 0 6 - - 0\n"
        "    paragraph 6 11 6 11 0\n"
        "    horizontal-rule 11 20 - - 0\n"
        "    paragraph 20 29 20 29 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a diary sexp is one line", "[org][paragraph]") {
    const std::string_view source =
        "%%(diary-float t 4 2)\n"
        "x\n";
    const std::string_view expected =
        "org-data 0 24 0 24 0\n"
        "  section 0 24 0 24 0\n"
        "    diary-sexp 0 22 - - 0\n"
        "    paragraph 22 24 22 24 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a LaTeX environment runs to its end line", "[org][paragraph]") {
    const std::string_view source =
        "\\begin{equation}\n"
        "x\n"
        "\\end{equation}\n"
        "\n"
        "y\n";
    const std::string_view expected =
        "org-data 0 37 0 37 0\n"
        "  section 0 37 0 37 0\n"
        "    latex-environment 0 35 - - 1\n"
        "    paragraph 35 37 35 37 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a LaTeX environment closed by another name is text",
    "[org][paragraph]") {
    const std::string_view source =
        "\\begin{a}\n"
        "x\n"
        "\\end{b}\n";
    const std::string_view expected =
        "org-data 0 20 0 20 0\n"
        "  section 0 20 0 20 0\n"
        "    paragraph 0 20 0 20 0\n"
        "      latex-fragment 0 9 - - 0\n"
        "      latex-fragment 12 19 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a clock line under a heading is a clock with its timestamp",
    "[org][paragraph]") {
    const std::string_view source =
        "* H\n"
        "CLOCK: [2024-01-01 Mon 10:00]--[2024-01-01 Mon 11:00] =>  1:00\n"
        "CLOCK: [2024-01-01 Mon 10:00]\n"
        "CLOCK: nope\n";
    const std::string_view expected =
        "org-data 0 109 0 109 0\n"
        "  headline 0 109 4 109 0 level=1 todo=- todo-type=- priority=- commen"
        "ted=0 tags=- title=H\n"
        "    section 4 109 4 109 0\n"
        "      clock 4 67 - - 0\n"
        "        timestamp 11 58 - - 1\n"
        "      clock 67 97 - - 0\n"
        "        timestamp 74 96 - - 0\n"
        "      paragraph 97 109 97 109 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a planning line follows its heading directly", "[org][paragraph]") {
    const std::string_view source =
        "* H\n"
        "DEADLINE: <2024-01-01 Mon> SCHEDULED: <2024-01-02 Tue>\n"
        "text\n";
    const std::string_view expected =
        "org-data 0 64 0 64 0\n"
        "  headline 0 64 4 64 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=H\n"
        "    section 4 64 4 64 0\n"
        "      planning 4 59 - - 0\n"
        "        timestamp 14 31 - - 1\n"
        "        timestamp 42 58 - - 0\n"
        "      paragraph 59 64 59 64 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a planning line after a blank line is a paragraph",
    "[org][paragraph]") {
    const std::string_view source =
        "* H\n"
        "\n"
        "DEADLINE: <2024-01-01 Mon>\n";
    const std::string_view expected =
        "org-data 0 32 0 32 0\n"
        "  headline 0 32 5 32 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=H\n"
        "    section 5 32 5 32 0\n"
        "      paragraph 5 32 5 32 0\n"
        "        timestamp 15 31 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a footnote definition ends at two blank lines",
    "[org][paragraph]") {
    const std::string_view source =
        "[fn:1] a\n"
        "b\n"
        "\n"
        "\n"
        "c\n";
    const std::string_view expected =
        "org-data 0 15 0 15 0\n"
        "  section 0 15 0 15 0\n"
        "    footnote-definition 0 13 7 11 2\n"
        "      paragraph 7 11 7 11 0\n"
        "    paragraph 13 15 13 15 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a footnote definition ends at the next definition",
    "[org][paragraph]") {
    const std::string_view source =
        "[fn:1] a\n"
        "[fn:2] b\n";
    const std::string_view expected =
        "org-data 0 18 0 18 0\n"
        "  section 0 18 0 18 0\n"
        "    footnote-definition 0 9 7 9 0\n"
        "      paragraph 7 9 7 9 0\n"
        "    footnote-definition 9 18 16 18 0\n"
        "      paragraph 16 18 16 18 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a footnote definition may start on the next line and ends "
          "at a heading",
    "[org][paragraph]") {
    const std::string_view source =
        "[fn:x]\n"
        "\n"
        "para\n"
        "* H\n";
    const std::string_view expected =
        "org-data 0 17 0 17 0\n"
        "  section 0 13 0 13 0\n"
        "    footnote-definition 0 13 8 13 0\n"
        "      paragraph 8 13 8 13 0\n"
        "  headline 13 17 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=H\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an indented footnote label is text", "[org][paragraph]") {
    const std::string_view source =
        " [fn:1] not a definition\n";
    const std::string_view expected =
        "org-data 0 25 0 25 0\n"
        "  section 0 25 0 25 0\n"
        "    paragraph 0 25 0 25 0\n"
        "      footnote-reference 1 8 - - 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a footnote definition holds elements", "[org][paragraph]") {
    const std::string_view source =
        "[fn:1] a\n"
        "\n"
        "- i\n"
        "\n"
        "#+begin_quote\n"
        "q\n"
        "#+end_quote\n";
    const std::string_view expected =
        "org-data 0 43 0 43 0\n"
        "  section 0 43 0 43 0\n"
        "    footnote-definition 0 43 7 43 0\n"
        "      paragraph 7 10 7 9 1\n"
        "      plain-list 10 15 10 14 1 type=unordered\n"
        "        item 10 14 12 14 0 checkbox=-\n"
        "          paragraph 12 14 12 14 0\n"
        "      quote-block 15 43 29 31 0\n"
        "        paragraph 29 31 29 31 0\n";
    CHECK(dump_of(source) == expected);
}

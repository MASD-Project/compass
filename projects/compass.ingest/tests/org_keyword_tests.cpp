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

TEST_CASE("a keyword has a key in lower case and a trimmed value",
    "[org][keyword]") {
    const std::string_view source =
        "#+title: T\n"
        "#+TITLE:   spaced  \n"
        "#+k:\n"
        "#+k:v:w\n"
        "#+a:b: c d\n";
    const std::string_view expected =
        "org-data 0 55 0 55 0\n"
        "  section 0 55 0 55 0\n"
        "    keyword 0 11 - - 0 key=title value=T\n"
        "    keyword 11 31 - - 0 key=title value=spaced\n"
        "    keyword 31 36 - - 0 key=k value=\n"
        "    keyword 36 44 - - 0 key=k:v value=w\n"
        "    keyword 44 55 - - 0 key=a:b value=c d\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a hash plus without a key and colon is text", "[org][keyword]") {
    const std::string_view source =
        "#+:\n"
        "#+ foo\n"
        "#+bar\n";
    const std::string_view expected =
        "org-data 0 17 0 17 0\n"
        "  section 0 17 0 17 0\n"
        "    paragraph 0 17 0 17 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a babel call is a keyword row named call", "[org][keyword]") {
    const std::string_view source =
        "#+call: f(x)\n"
        "#+CALL:g()\n";
    const std::string_view expected =
        "org-data 0 24 0 24 0\n"
        "  section 0 24 0 24 0\n"
        "    babel-call 0 13 - - 0 key=call value=f(x)\n"
        "    babel-call 13 24 - - 0 key=call value=g()\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an affiliated keyword attaches to the element after it",
    "[org][keyword]") {
    const std::string_view source =
        "#+name: n\n"
        "para\n";
    const std::string_view expected =
        "org-data 0 15 0 15 0\n"
        "  section 0 15 0 15 0\n"
        "    paragraph 0 15 10 15 0\n"
        "      affiliated key=name value=n\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an affiliated keyword before a blank line is a keyword",
    "[org][keyword]") {
    const std::string_view source =
        "#+name: n\n"
        "\n"
        "para\n";
    const std::string_view expected =
        "org-data 0 16 0 16 0\n"
        "  section 0 16 0 16 0\n"
        "    keyword 0 11 - - 1 key=name value=n\n"
        "    paragraph 11 16 11 16 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a caption is parsed and attributes repeat", "[org][keyword]") {
    const std::string_view source =
        "#+caption: c *b* [[id:x][l]]\n"
        "#+attr_html: :w 1\n"
        "#+attr_html: :h 2\n"
        "| t |\n";
    const std::string_view expected =
        "org-data 0 71 0 71 0\n"
        "  section 0 71 0 71 0\n"
        "    table 0 71 65 71 0 type=org\n"
        "      affiliated key=attr_html value=:w 1\n"
        "      affiliated key=attr_html value=:h 2\n"
        "      affiliated key=caption\n"
        "        bold 13 17 14 15 1\n"
        "        link 17 28 25 26 0 type=id format=bracket path=x\n"
        "      table-row 65 71 66 70 0 type=standard\n"
        "        table-cell 66 70 67 68 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a caption may carry a short form", "[org][keyword]") {
    const std::string_view source =
        "#+caption[short *s*]: long /l/\n"
        "para\n";
    const std::string_view expected =
        "org-data 0 36 0 36 0\n"
        "  section 0 36 0 36 0\n"
        "    paragraph 0 36 31 36 0\n"
        "      affiliated key=caption\n"
        "        italic 27 30 28 29 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a results keyword may carry a hash", "[org][keyword]") {
    const std::string_view source =
        "#+results[abc]: n\n"
        ": x\n";
    const std::string_view expected =
        "org-data 0 22 0 22 0\n"
        "  section 0 22 0 22 0\n"
        "    fixed-width 0 22 - - 1\n"
        "      affiliated key=results value=n\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("old names of name are translated", "[org][keyword]") {
    const std::string_view source =
        "#+tblname: t\n"
        "| a |\n"
        "\n"
        "#+label: l\n"
        "para\n"
        "\n"
        "#+srcname: s\n"
        "#+begin_src c\n"
        "#+end_src\n";
    const std::string_view expected =
        "org-data 0 74 0 74 0\n"
        "  section 0 74 0 74 0\n"
        "    table 0 20 13 19 1 type=org\n"
        "      affiliated key=name value=t\n"
        "      table-row 13 19 14 18 0 type=standard\n"
        "        table-cell 14 18 15 16 0\n"
        "    paragraph 20 37 31 36 1\n"
        "      affiliated key=name value=l\n"
        "    src-block 37 74 - - 0 name=c parameters=\n"
        "      affiliated key=name value=s\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("the last of two name keywords wins", "[org][keyword]") {
    const std::string_view source =
        "#+name: a\n"
        "#+name: b\n"
        "para\n";
    const std::string_view expected =
        "org-data 0 25 0 25 0\n"
        "  section 0 25 0 25 0\n"
        "    paragraph 0 25 20 25 0\n"
        "      affiliated key=name value=b\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("affiliated keywords before a comment or a heading are keywords",
    "[org][keyword]") {
    const std::string_view source =
        "#+name: n\n"
        "# c\n"
        "#+name: m\n"
        "* H\n";
    const std::string_view expected =
        "org-data 0 28 0 28 0\n"
        "  section 0 24 0 24 0\n"
        "    keyword 0 10 - - 0 key=name value=n\n"
        "    comment 10 14 - - 0\n"
        "    keyword 14 24 - - 0 key=name value=m\n"
        "  headline 24 28 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=H\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an affiliated keyword at the end of the file is a keyword",
    "[org][keyword]") {
    const std::string_view source =
        "#+name: n";
    const std::string_view expected =
        "org-data 0 9 0 9 0\n"
        "  section 0 9 0 9 0\n"
        "    keyword 0 9 - - 0 key=name value=n\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a header keyword attaches to a block", "[org][keyword]") {
    const std::string_view source =
        "#+header: :x 1\n"
        "#+headers: :y 2\n"
        "#+begin_src c\n"
        "#+end_src\n";
    const std::string_view expected =
        "org-data 0 55 0 55 0\n"
        "  section 0 55 0 55 0\n"
        "    src-block 0 55 - - 0 name=c parameters=\n"
        "      affiliated key=header value=:x 1\n"
        "      affiliated key=header value=:y 2\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an affiliated keyword may attach to a keyword", "[org][keyword]") {
    const std::string_view source =
        "#+caption: x\n"
        "#+title: t\n";
    const std::string_view expected =
        "org-data 0 24 0 24 0\n"
        "  section 0 24 0 24 0\n"
        "    keyword 0 24 - - 0 key=title value=t\n"
        "      affiliated key=caption\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an affiliated keyword may attach to a list", "[org][keyword]") {
    const std::string_view source =
        "#+name: l\n"
        "- a\n"
        "- b\n";
    const std::string_view expected =
        "org-data 0 18 0 18 0\n"
        "  section 0 18 0 18 0\n"
        "    plain-list 0 18 10 18 0 type=unordered\n"
        "      affiliated key=name value=l\n"
        "      item 10 14 12 14 0 checkbox=-\n"
        "        paragraph 12 14 12 14 0\n"
        "      item 14 18 16 18 0 checkbox=-\n"
        "        paragraph 16 18 16 18 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an attribute keyword without a value has an empty value",
    "[org][keyword]") {
    const std::string_view source =
        "#+attr_html:\n"
        "para\n";
    const std::string_view expected =
        "org-data 0 18 0 18 0\n"
        "  section 0 18 0 18 0\n"
        "    paragraph 0 18 13 18 0\n"
        "      affiliated key=attr_html value=\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an affiliated keyword inside an item attaches to the paragraph",
    "[org][keyword]") {
    const std::string_view source =
        "- a\n"
        "  #+name: x\n"
        "  para\n";
    const std::string_view expected =
        "org-data 0 23 0 23 0\n"
        "  section 0 23 0 23 0\n"
        "    plain-list 0 23 0 23 0 type=unordered\n"
        "      item 0 23 2 23 0 checkbox=-\n"
        "        paragraph 2 4 2 4 0\n"
        "        paragraph 4 23 16 23 0\n"
        "          affiliated key=name value=x\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a caption of a table inside an item", "[org][keyword]") {
    const std::string_view source =
        "- a\n"
        "\n"
        "  #+caption: c\n"
        "  | t |\n";
    const std::string_view expected =
        "org-data 0 28 0 28 0\n"
        "  section 0 28 0 28 0\n"
        "    plain-list 0 28 0 28 0 type=unordered\n"
        "      item 0 28 2 28 0 checkbox=-\n"
        "        paragraph 2 5 2 4 1\n"
        "        table 5 28 20 28 0 type=org\n"
        "          affiliated key=caption\n"
        "          table-row 20 28 23 27 0 type=standard\n"
        "            table-cell 23 27 24 25 0\n";
    CHECK(dump_of(source) == expected);
}

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

TEST_CASE("an empty document is one document row", "[org][document]") {
    const std::string_view source =
        "";
    const std::string_view expected =
        "org-data 0 0 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a document of blank lines has no section", "[org][document]") {
    const std::string_view source =
        "\n"
        "\n";
    const std::string_view expected =
        "org-data 0 2 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("blank lines before the first text stay outside the section",
    "[org][document]") {
    const std::string_view source =
        "\n"
        "\n"
        "hello\n";
    const std::string_view expected =
        "org-data 0 8 2 8 0\n"
        "  section 2 8 2 8 0\n"
        "    paragraph 2 8 2 8 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("text without a final line feed ends at the end of the file",
    "[org][document]") {
    const std::string_view source =
        "hello";
    const std::string_view expected =
        "org-data 0 5 0 5 0\n"
        "  section 0 5 0 5 0\n"
        "    paragraph 0 5 0 5 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("blank lines after a paragraph are its post-blank",
    "[org][document]") {
    const std::string_view source =
        "a\n"
        "\n"
        "\n";
    const std::string_view expected =
        "org-data 0 4 0 4 0\n"
        "  section 0 4 0 4 0\n"
        "    paragraph 0 4 0 2 2\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a heading splits the document into a section and a headline",
    "[org][document]") {
    const std::string_view source =
        "intro\n"
        "* H\n"
        "body\n";
    const std::string_view expected =
        "org-data 0 15 0 15 0\n"
        "  section 0 6 0 6 0\n"
        "    paragraph 0 6 0 6 0\n"
        "  headline 6 15 10 15 0 level=1 todo=- todo-type=- priority=- comment"
        "ed=0 tags=- title=H\n"
        "    section 10 15 10 15 0\n"
        "      paragraph 10 15 10 15 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a heading without a line feed has no contents", "[org][document]") {
    const std::string_view source =
        "* H";
    const std::string_view expected =
        "org-data 0 3 0 3 0\n"
        "  headline 0 3 - - 0 level=1 todo=- todo-type=- priority=- commented="
        "0 tags=- title=H\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("emphasis at the start of a line is not a heading",
    "[org][document]") {
    const std::string_view source =
        "*not* a heading\n";
    const std::string_view expected =
        "org-data 0 16 0 16 0\n"
        "  section 0 16 0 16 0\n"
        "    paragraph 0 16 0 16 0\n"
        "      bold 0 6 1 4 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("stars with no space after them are text", "[org][document]") {
    const std::string_view source =
        "*\n"
        "**\n";
    const std::string_view expected =
        "org-data 0 5 0 5 0\n"
        "  section 0 5 0 5 0\n"
        "    paragraph 0 5 0 5 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("headings nest by level", "[org][document]") {
    const std::string_view source =
        "* A\n"
        "** B\n"
        "*** C\n"
        "* D\n";
    const std::string_view expected =
        "org-data 0 19 0 19 0\n"
        "  headline 0 15 4 15 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=A\n"
        "    headline 4 15 9 15 0 level=2 todo=- todo-type=- priority=- commen"
        "ted=0 tags=- title=B\n"
        "      headline 9 15 - - 0 level=3 todo=- todo-type=- priority=- comme"
        "nted=0 tags=- title=C\n"
        "  headline 15 19 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=D\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a deeper first heading is closed by a shallower one",
    "[org][document]") {
    const std::string_view source =
        "*** A\n"
        "* B\n"
        "** C\n";
    const std::string_view expected =
        "org-data 0 15 0 15 0\n"
        "  headline 0 6 - - 0 level=3 todo=- todo-type=- priority=- commented="
        "0 tags=- title=A\n"
        "  headline 6 15 10 15 0 level=1 todo=- todo-type=- priority=- comment"
        "ed=0 tags=- title=B\n"
        "    headline 10 15 - - 0 level=2 todo=- todo-type=- priority=- commen"
        "ted=0 tags=- title=C\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("blank lines after a heading without contents are its post-blank",
    "[org][document]") {
    const std::string_view source =
        "* A\n"
        "\n"
        "\n"
        "* B\n"
        "\n";
    const std::string_view expected =
        "org-data 0 11 0 11 0\n"
        "  headline 0 6 - - 2 level=1 todo=- todo-type=- priority=- commented="
        "0 tags=- title=A\n"
        "  headline 6 11 - - 1 level=1 todo=- todo-type=- priority=- commented"
        "=0 tags=- title=B\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("TODO and DONE are the default vocabulary", "[org][document]") {
    const std::string_view source =
        "* TODO a\n"
        "* DONE b\n"
        "* NEXT c\n";
    const std::string_view expected =
        "org-data 0 27 0 27 0\n"
        "  headline 0 9 - - 0 level=1 todo=TODO todo-type=todo priority=- comm"
        "ented=0 tags=- title=a\n"
        "  headline 9 18 - - 0 level=1 todo=DONE todo-type=done priority=- com"
        "mented=0 tags=- title=b\n"
        "  headline 18 27 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=NEXT c\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a todo keyword line replaces the vocabulary and strips the keys",
    "[org][document]") {
    const std::string_view source =
        "#+todo: OPEN(o) WIP | CLOSED(c@/!)\n"
        "* OPEN a\n"
        "* CLOSED b\n"
        "* TODO c\n"
        "* WIP d\n";
    const std::string_view expected =
        "org-data 0 72 0 72 0\n"
        "  section 0 35 0 35 0\n"
        "    keyword 0 35 - - 0 key=todo value=OPEN(o) WIP | CLOSED(c@/!)\n"
        "  headline 35 44 - - 0 level=1 todo=OPEN todo-type=todo priority=- co"
        "mmented=0 tags=- title=a\n"
        "  headline 44 55 - - 0 level=1 todo=CLOSED todo-type=done priority=- "
        "commented=0 tags=- title=b\n"
        "  headline 55 64 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=TODO c\n"
        "  headline 64 72 - - 0 level=1 todo=WIP todo-type=todo priority=- com"
        "mented=0 tags=- title=d\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("without a bar the last keyword is the done state",
    "[org][document]") {
    const std::string_view source =
        "#+todo: A B C\n"
        "* C x\n"
        "* B y\n";
    const std::string_view expected =
        "org-data 0 26 0 26 0\n"
        "  section 0 14 0 14 0\n"
        "    keyword 0 14 - - 0 key=todo value=A B C\n"
        "  headline 14 20 - - 0 level=1 todo=C todo-type=done priority=- comme"
        "nted=0 tags=- title=x\n"
        "  headline 20 26 - - 0 level=1 todo=B todo-type=todo priority=- comme"
        "nted=0 tags=- title=y\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a todo keyword line below the headings still applies",
    "[org][document]") {
    const std::string_view source =
        "* NEXT a\n"
        "#+seq_todo: NEXT | FIN\n"
        "* FIN b\n";
    const std::string_view expected =
        "org-data 0 40 0 40 0\n"
        "  headline 0 32 9 32 0 level=1 todo=NEXT todo-type=todo priority=- co"
        "mmented=0 tags=- title=a\n"
        "    section 9 32 9 32 0\n"
        "      keyword 9 32 - - 0 key=seq_todo value=NEXT | FIN\n"
        "  headline 32 40 - - 0 level=1 todo=FIN todo-type=done priority=- com"
        "mented=0 tags=- title=b\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a todo line inside an example block defines nothing",
    "[org][document]") {
    const std::string_view source =
        "#+begin_example\n"
        "#+todo: X | Y\n"
        "#+end_example\n"
        "* X a\n";
    const std::string_view expected =
        "org-data 0 50 0 50 0\n"
        "  section 0 44 0 44 0\n"
        "    example-block 0 44 - - 0 parameters=\n"
        "  headline 44 50 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=X a\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a todo keyword needs a space or the end of the line after it",
    "[org][document]") {
    const std::string_view source =
        "* TODOx\n"
        "* TODO\n"
        "* TODO\tx\n";
    const std::string_view expected =
        "org-data 0 24 0 24 0\n"
        "  headline 0 8 - - 0 level=1 todo=- todo-type=- priority=- commented="
        "0 tags=- title=TODOx\n"
        "  headline 8 15 - - 0 level=1 todo=TODO todo-type=todo priority=- com"
        "mented=0 tags=- title=\n"
        "  headline 15 24 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=TODO\\tx\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a priority cookie follows the todo keyword", "[org][document]") {
    const std::string_view source =
        "* [#A] a\n"
        "* TODO [#B] b\n"
        "* [#1] c\n"
        "* [#AB] d\n"
        "* x [#A]\n";
    const std::string_view expected =
        "org-data 0 51 0 51 0\n"
        "  headline 0 9 - - 0 level=1 todo=- todo-type=- priority=65 commented"
        "=0 tags=- title=a\n"
        "  headline 9 23 - - 0 level=1 todo=TODO todo-type=todo priority=66 co"
        "mmented=0 tags=- title=b\n"
        "  headline 23 32 - - 0 level=1 todo=- todo-type=- priority=49 comment"
        "ed=0 tags=- title=c\n"
        "  headline 32 42 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=[#AB] d\n"
        "  headline 42 51 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=x [#A]\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("COMMENT marks a heading only as a whole word in upper case",
    "[org][document]") {
    const std::string_view source =
        "* COMMENT a\n"
        "* COMMENTa\n"
        "* TODO COMMENT b\n"
        "* comment c\n"
        "* COMMENT\n";
    const std::string_view expected =
        "org-data 0 62 0 62 0\n"
        "  headline 0 12 - - 0 level=1 todo=- todo-type=- priority=- commented"
        "=1 tags=- title=a\n"
        "  headline 12 23 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=COMMENTa\n"
        "  headline 23 40 - - 0 level=1 todo=TODO todo-type=todo priority=- co"
        "mmented=1 tags=- title=b\n"
        "  headline 40 52 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=comment c\n"
        "  headline 52 62 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=1 tags=- title=\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("tags end the heading line", "[org][document]") {
    const std::string_view source =
        "* a :x:y:\n"
        "* b :x: \n"
        "* c:x:\n"
        "* :x:\n"
        "* d :x y:\n"
        "* e :x:y\n"
        "* f ::\n"
        "* g :@a#b%c_d:\n"
        "* h :\303\251:\n";
    const std::string_view expected =
        "org-data 0 82 0 82 0\n"
        "  headline 0 10 - - 0 level=1 todo=- todo-type=- priority=- commented"
        "=0 tags=x:y title=a\n"
        "  headline 10 19 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=x title=b\n"
        "  headline 19 26 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=x title=c\n"
        "  headline 26 32 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=x title=\n"
        "  headline 32 42 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=d :x y:\n"
        "  headline 42 51 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=e :x:y\n"
        "  headline 51 58 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=f ::\n"
        "  headline 58 73 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=@a#b%c_d title=g\n"
        "  headline 73 82 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=\303\251 title=h\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("the title of a heading holds objects but no line break",
    "[org][document]") {
    const std::string_view source =
        "* A *b* [[id:x][y]] =v= \\\\\n";
    const std::string_view expected =
        "org-data 0 27 0 27 0\n"
        "  headline 0 27 - - 0 level=1 todo=- todo-type=- priority=- commented"
        "=0 tags=- title=A *b* [[id:x][y]] =v= \\\\\\\\\n"
        "    bold 4 8 5 6 1\n"
        "    link 8 20 16 17 1 type=id format=bracket path=x\n"
        "    verbatim 20 24 - - 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a heading with only a keyword has an empty title",
    "[org][document]") {
    const std::string_view source =
        "* TODO\n"
        "* TODO \n"
        "*  \n";
    const std::string_view expected =
        "org-data 0 19 0 19 0\n"
        "  headline 0 7 - - 0 level=1 todo=TODO todo-type=todo priority=- comm"
        "ented=0 tags=- title=\n"
        "  headline 7 15 - - 0 level=1 todo=TODO todo-type=todo priority=- com"
        "mented=0 tags=- title=\n"
        "  headline 15 19 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a heading with a keyword, a priority, a title with a link and tags",
    "[org][document]") {
    const std::string_view source =
        "* TODO [#A] See [[id:x][the *doc*]] now :a:b:\n";
    const std::string_view expected =
        "org-data 0 46 0 46 0\n"
        "  headline 0 46 - - 0 level=1 todo=TODO todo-type=todo priority=65 co"
        "mmented=0 tags=a:b title=See [[id:x][the *doc*]] now\n"
        "    link 16 36 24 33 1 type=id format=bracket path=x\n"
        "      bold 28 33 29 32 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("headings six levels deep", "[org][document]") {
    const std::string_view source =
        "* 1\n"
        "** 2\n"
        "*** 3\n"
        "**** 4\n"
        "***** 5\n"
        "****** 6\n";
    const std::string_view expected =
        "org-data 0 39 0 39 0\n"
        "  headline 0 39 4 39 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=1\n"
        "    headline 4 39 9 39 0 level=2 todo=- todo-type=- priority=- commen"
        "ted=0 tags=- title=2\n"
        "      headline 9 39 15 39 0 level=3 todo=- todo-type=- priority=- com"
        "mented=0 tags=- title=3\n"
        "        headline 15 39 22 39 0 level=4 todo=- todo-type=- priority=- "
        "commented=0 tags=- title=4\n"
        "          headline 22 39 30 39 0 level=5 todo=- todo-type=- priority="
        "- commented=0 tags=- title=5\n"
        "            headline 30 39 - - 0 level=6 todo=- todo-type=- priority="
        "- commented=0 tags=- title=6\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a section ends where the next heading starts", "[org][document]") {
    const std::string_view source =
        "* A\n"
        "text\n"
        "\n"
        "** B\n"
        "more\n"
        "* C\n";
    const std::string_view expected =
        "org-data 0 24 0 24 0\n"
        "  headline 0 20 4 20 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=A\n"
        "    section 4 10 4 10 0\n"
        "      paragraph 4 10 4 9 1\n"
        "    headline 10 20 15 20 0 level=2 todo=- todo-type=- priority=- comm"
        "ented=0 tags=- title=B\n"
        "      section 15 20 15 20 0\n"
        "        paragraph 15 20 15 20 0\n"
        "  headline 20 24 - - 0 level=1 todo=- todo-type=- priority=- commente"
        "d=0 tags=- title=C\n";
    CHECK(dump_of(source) == expected);
}

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

TEST_CASE("a source block has a language and parameters", "[org][block]") {
    const std::string_view source =
        "#+begin_src cpp -n :tangle x\n"
        "int x;\n"
        "#+end_src\n";
    const std::string_view expected =
        "org-data 0 46 0 46 0\n"
        "  section 0 46 0 46 0\n"
        "    src-block 0 46 - - 0 name=cpp parameters=-n :tangle x\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a source block without a language", "[org][block]") {
    const std::string_view source =
        "#+BEGIN_SRC\n"
        "#+END_SRC\n";
    const std::string_view expected =
        "org-data 0 22 0 22 0\n"
        "  section 0 22 0 22 0\n"
        "    src-block 0 22 - - 0 name=- parameters=\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a line that looks like a heading ends the section and "
          "unbalances the block",
    "[org][block]") {
    const std::string_view source =
        "#+begin_src sh\n"
        "* heading\n"
        "#+end_src\n";
    const std::string_view expected =
        "org-data 0 35 0 35 0\n"
        "  section 0 15 0 15 0\n"
        "    paragraph 0 15 0 15 0\n"
        "      subscript 7 12 8 11 1\n"
        "  headline 15 35 25 35 0 level=1 todo=- todo-type=- priority=- commen"
        "ted=0 tags=- title=heading\n"
        "    section 25 35 25 35 0\n"
        "      paragraph 25 35 25 35 0\n"
        "        subscript 30 34 31 34 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an escaped or indented star line stays inside the block",
    "[org][block]") {
    const std::string_view source =
        "#+begin_src sh\n"
        ",* x\n"
        " * y\n"
        "*z\n"
        "#+end_src\n";
    const std::string_view expected =
        "org-data 0 38 0 38 0\n"
        "  section 0 38 0 38 0\n"
        "    src-block 0 38 - - 0 name=sh parameters=\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an example block keeps its switches", "[org][block]") {
    const std::string_view source =
        "#+begin_example -n -r\n"
        "  text\n"
        "#+end_example\n"
        "\n";
    const std::string_view expected =
        "org-data 0 44 0 44 0\n"
        "  section 0 44 0 44 0\n"
        "    example-block 0 44 - - 1 parameters=-n -r\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("the first matching end line closes the block", "[org][block]") {
    const std::string_view source =
        "#+begin_example\n"
        "#+begin_src c\n"
        "#+end_src\n"
        "#+end_example\n";
    const std::string_view expected =
        "org-data 0 54 0 54 0\n"
        "  section 0 54 0 54 0\n"
        "    example-block 0 54 - - 0 parameters=\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a block that is not closed is a paragraph", "[org][block]") {
    const std::string_view source =
        "#+begin_src c\n"
        "x\n";
    const std::string_view expected =
        "org-data 0 16 0 16 0\n"
        "  section 0 16 0 16 0\n"
        "    paragraph 0 16 0 16 0\n"
        "      subscript 7 12 8 11 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a block closed by another kind is a paragraph", "[org][block]") {
    const std::string_view source =
        "#+begin_src c\n"
        "x\n"
        "#+end_example\n";
    const std::string_view expected =
        "org-data 0 30 0 30 0\n"
        "  section 0 30 0 30 0\n"
        "    paragraph 0 30 0 30 0\n"
        "      subscript 7 12 8 11 1\n"
        "      subscript 21 29 22 29 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an end line with text after it does not close", "[org][block]") {
    const std::string_view source =
        "#+begin_src c\n"
        "#+end_src x\n"
        "#+end_src\n";
    const std::string_view expected =
        "org-data 0 36 0 36 0\n"
        "  section 0 36 0 36 0\n"
        "    src-block 0 36 - - 0 name=c parameters=\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a quote block holds elements", "[org][block]") {
    const std::string_view source =
        "#+begin_quote\n"
        "para\n"
        "\n"
        "- item\n"
        "#+end_quote\n";
    const std::string_view expected =
        "org-data 0 39 0 39 0\n"
        "  section 0 39 0 39 0\n"
        "    quote-block 0 39 14 27 0\n"
        "      paragraph 14 20 14 19 1\n"
        "      plain-list 20 27 20 27 0 type=unordered\n"
        "        item 20 27 22 27 0 checkbox=-\n"
        "          paragraph 22 27 22 27 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an empty quote block has no contents", "[org][block]") {
    const std::string_view source =
        "#+begin_quote\n"
        "#+end_quote\n";
    const std::string_view expected =
        "org-data 0 26 0 26 0\n"
        "  section 0 26 0 26 0\n"
        "    quote-block 0 26 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a center block holds elements", "[org][block]") {
    const std::string_view source =
        "#+begin_center\n"
        "c\n"
        "#+end_center\n";
    const std::string_view expected =
        "org-data 0 30 0 30 0\n"
        "  section 0 30 0 30 0\n"
        "    center-block 0 30 15 17 0\n"
        "      paragraph 15 17 15 17 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a verse block holds objects", "[org][block]") {
    const std::string_view source =
        "#+begin_verse\n"
        " *b* line\n"
        "  two\n"
        "#+end_verse\n";
    const std::string_view expected =
        "org-data 0 42 0 42 0\n"
        "  section 0 42 0 42 0\n"
        "    verse-block 0 42 14 30 0\n"
        "      bold 15 19 16 17 1\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an export block has a backend in lower case", "[org][block]") {
    const std::string_view source =
        "#+begin_export HTML\n"
        "<b>\n"
        "#+end_export\n"
        "\n"
        "#+begin_export\n"
        "#+end_export\n";
    const std::string_view expected =
        "org-data 0 66 0 66 0\n"
        "  section 0 66 0 66 0\n"
        "    export-block 0 38 - - 1 name=html\n"
        "    export-block 38 66 - - 0 name=-\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a comment block is opaque", "[org][block]") {
    const std::string_view source =
        "#+begin_comment\n"
        "*b*\n"
        "#+end_comment\n";
    const std::string_view expected =
        "org-data 0 34 0 34 0\n"
        "  section 0 34 0 34 0\n"
        "    comment-block 0 34 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("any other name is a special block", "[org][block]") {
    const std::string_view source =
        "#+begin_note\n"
        "x\n"
        "#+end_note\n"
        "\n"
        "#+begin_Note with params\n"
        "y\n"
        "#+END_NOTE\n";
    const std::string_view expected =
        "org-data 0 65 0 65 0\n"
        "  section 0 65 0 65 0\n"
        "    special-block 0 27 13 15 1 name=note parameters=\n"
        "      paragraph 13 15 13 15 0\n"
        "    special-block 27 65 52 54 0 name=Note parameters=with params\n"
        "      paragraph 52 54 52 54 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a dynamic block has a name and arguments", "[org][block]") {
    const std::string_view source =
        "#+begin: clocktable :scope file\n"
        "x\n"
        "#+end:\n"
        "\n"
        "#+BEGIN: name\n"
        "#+END\n";
    const std::string_view expected =
        "org-data 0 62 0 62 0\n"
        "  section 0 62 0 62 0\n"
        "    dynamic-block 0 42 32 34 1 name=clocktable parameters=:scope file"
        "\n"
        "      paragraph 32 34 32 34 0\n"
        "    dynamic-block 42 62 - - 0 name=name parameters=\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a block may be indented", "[org][block]") {
    const std::string_view source =
        "  #+begin_src c\n"
        "  x\n"
        "  #+end_src\n";
    const std::string_view expected =
        "org-data 0 32 0 32 0\n"
        "  section 0 32 0 32 0\n"
        "    src-block 0 32 - - 0 name=c parameters=\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("blank lines after a block are its post-blank", "[org][block]") {
    const std::string_view source =
        "#+begin_src c\n"
        "#+end_src\n"
        "\n"
        "\n"
        "x\n";
    const std::string_view expected =
        "org-data 0 28 0 28 0\n"
        "  section 0 28 0 28 0\n"
        "    src-block 0 26 - - 2 name=c parameters=\n"
        "    paragraph 26 28 26 28 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a block name runs to the next space", "[org][block]") {
    const std::string_view source =
        "#+begin_src:\n"
        "#+end_src:\n"
        "\n"
        "#+begin_\n"
        "#+end_\n";
    const std::string_view expected =
        "org-data 0 41 0 41 0\n"
        "  section 0 41 0 41 0\n"
        "    special-block 0 25 - - 1 name=src: parameters=\n"
        "    paragraph 25 41 25 41 0\n";
    CHECK(dump_of(source) == expected);
}

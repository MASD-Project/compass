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

TEST_CASE("a bracket link with a type", "[org][link]") {
    const std::string_view source =
        "[[id:abc]]\n";
    const std::string_view expected =
        "org-data 0 11 0 11 0\n"
        "  section 0 11 0 11 0\n"
        "    paragraph 0 11 0 11 0\n"
        "      link 0 10 - - 0 type=id format=bracket path=abc\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a description holds objects", "[org][link]") {
    const std::string_view source =
        "[[id:abc][desc *b* =v=]]\n";
    const std::string_view expected =
        "org-data 0 25 0 25 0\n"
        "  section 0 25 0 25 0\n"
        "    paragraph 0 25 0 25 0\n"
        "      link 0 24 10 22 0 type=id format=bracket path=abc\n"
        "        bold 15 19 16 17 1\n"
        "        verbatim 19 22 - - 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a web link keeps its slashes in the path", "[org][link]") {
    const std::string_view source =
        "[[https://x.y/z\?a=b][d]]\n";
    const std::string_view expected =
        "org-data 0 25 0 25 0\n"
        "  section 0 25 0 25 0\n"
        "    paragraph 0 25 0 25 0\n"
        "      link 0 24 21 22 0 type=https format=bracket path=//x.y/z\?a=b\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a file link keeps its search option", "[org][link]") {
    const std::string_view source =
        "[[file:a.org::*H][d]] [[file:b.png]]\n";
    const std::string_view expected =
        "org-data 0 37 0 37 0\n"
        "  section 0 37 0 37 0\n"
        "    paragraph 0 37 0 37 0\n"
        "      link 0 22 18 19 1 type=file format=bracket path=a.org::*H\n"
        "      link 22 36 - - 0 type=file format=bracket path=b.png\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a relative or absolute path is a file link", "[org][link]") {
    const std::string_view source =
        "[[./a.png]] [[../b]] [[/abs/p]] [[~/p]]\n";
    const std::string_view expected =
        "org-data 0 40 0 40 0\n"
        "  section 0 40 0 40 0\n"
        "    paragraph 0 40 0 40 0\n"
        "      link 0 12 - - 1 type=file format=bracket path=./a.png\n"
        "      link 12 21 - - 1 type=file format=bracket path=../b\n"
        "      link 21 32 - - 1 type=file format=bracket path=/abs/p\n"
        "      link 32 39 - - 0 type=file format=bracket path=~/p\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("custom ids, code references and fuzzy links", "[org][link]") {
    const std::string_view source =
        "[[#custom]] [[(ref)]] [[fuzzy target]] [[*Heading]]\n";
    const std::string_view expected =
        "org-data 0 52 0 52 0\n"
        "  section 0 52 0 52 0\n"
        "    paragraph 0 52 0 52 0\n"
        "      link 0 12 - - 1 type=custom-id format=bracket path=custom\n"
        "      link 12 22 - - 1 type=coderef format=bracket path=ref\n"
        "      link 22 39 - - 1 type=fuzzy format=bracket path=fuzzy target\n"
        "      link 39 51 - - 0 type=fuzzy format=bracket path=*Heading\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("an unregistered scheme is the type", "[org][link]") {
    const std::string_view source =
        "[[proj:a/b][d]] [[std::vector]] [[Note: x]] [[x:y]]\n";
    const std::string_view expected =
        "org-data 0 52 0 52 0\n"
        "  section 0 52 0 52 0\n"
        "    paragraph 0 52 0 52 0\n"
        "      link 0 16 12 13 1 type=proj format=bracket path=a/b\n"
        "      link 16 32 - - 1 type=fuzzy format=bracket path=std::vector\n"
        "      link 32 44 - - 1 type=fuzzy format=bracket path=Note: x\n"
        "      link 44 51 - - 0 type=x format=bracket path=y\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("brackets in a path are escaped", "[org][link]") {
    const std::string_view source =
        "[[a\\]b]] [[a\\\\]] [[a[b]]\n";
    const std::string_view expected =
        "org-data 0 25 0 25 0\n"
        "  section 0 25 0 25 0\n"
        "    paragraph 0 25 0 25 0\n"
        "      link 0 9 - - 1 type=fuzzy format=bracket path=a]b\n"
        "      link 9 17 - - 1 type=fuzzy format=bracket path=a\\\\\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("what is not a link", "[org][link]") {
    const std::string_view source =
        "[[]] [[a] [[a][]] [a]\n";
    const std::string_view expected =
        "org-data 0 22 0 22 0\n"
        "  section 0 22 0 22 0\n"
        "    paragraph 0 22 0 22 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("the description ends at the first double bracket", "[org][link]") {
    const std::string_view source =
        "[[a][d]] ]] [[a][b]c]]\n";
    const std::string_view expected =
        "org-data 0 23 0 23 0\n"
        "  section 0 23 0 23 0\n"
        "    paragraph 0 23 0 23 0\n"
        "      link 0 9 5 6 1 type=fuzzy format=bracket path=a\n"
        "      link 12 22 17 20 0 type=fuzzy format=bracket path=a\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a link wraps a line", "[org][link]") {
    const std::string_view source =
        "[[a b\n"
        "  c][d\n"
        "e]]\n";
    const std::string_view expected =
        "org-data 0 17 0 17 0\n"
        "  section 0 17 0 17 0\n"
        "    paragraph 0 17 0 17 0\n"
        "      link 0 16 11 14 0 type=fuzzy format=bracket path=a b c\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("plain links of registered types", "[org][link]") {
    const std::string_view source =
        "http://x.y/z. https://a.b/c(d)e, mailto:a@b.c file:a.org id:abc\n";
    const std::string_view expected =
        "org-data 0 64 0 64 0\n"
        "  section 0 64 0 64 0\n"
        "    paragraph 0 64 0 64 0\n"
        "      link 0 12 - - 0 type=http format=plain path=//x.y/z\n"
        "      link 14 31 - - 0 type=https format=plain path=//a.b/c(d)e\n"
        "      link 33 46 - - 1 type=mailto format=plain path=a@b.c\n"
        "      link 46 57 - - 1 type=file format=plain path=a.org\n"
        "      link 57 63 - - 0 type=id format=plain path=abc\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("what is not a plain link", "[org][link]") {
    const std::string_view source =
        "xhttp://n proj:x http:a http: //x\n";
    const std::string_view expected =
        "org-data 0 34 0 34 0\n"
        "  section 0 34 0 34 0\n"
        "    paragraph 0 34 0 34 0\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a plain link drops trailing punctuation", "[org][link]") {
    const std::string_view source =
        "(https://x.y/z) https://x.y/z/ end https://x.y/a.b.\n";
    const std::string_view expected =
        "org-data 0 52 0 52 0\n"
        "  section 0 52 0 52 0\n"
        "    paragraph 0 52 0 52 0\n"
        "      link 1 14 - - 0 type=https format=plain path=//x.y/z\n"
        "      link 16 31 - - 1 type=https format=plain path=//x.y/z/\n"
        "      link 35 50 - - 0 type=https format=plain path=//x.y/a.b\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("a plain link keeps the case of its type but file is file",
    "[org][link]") {
    const std::string_view source =
        "HTTPS://X.Y File:line\n";
    const std::string_view expected =
        "org-data 0 22 0 22 0\n"
        "  section 0 22 0 22 0\n"
        "    paragraph 0 22 0 22 0\n"
        "      link 0 12 - - 1 type=HTTPS format=plain path=//X.Y\n"
        "      link 12 21 - - 0 type=file format=plain path=line\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("angle links", "[org][link]") {
    const std::string_view source =
        "<https://x.y/z> <http://a b> <proj:x> <id:a\n"
        "b>\n";
    const std::string_view expected =
        "org-data 0 47 0 47 0\n"
        "  section 0 47 0 47 0\n"
        "    paragraph 0 47 0 47 0\n"
        "      link 0 16 - - 1 type=https format=angle path=//x.y/z\n"
        "      link 16 29 - - 1 type=http format=angle path=//a b\n"
        "      link 38 46 - - 0 type=id format=angle path=ab\n";
    CHECK(dump_of(source) == expected);
}

TEST_CASE("the same link type has one name", "[org][link]") {
    const std::string_view source =
        "[[id:a]] [[id:b]] id:cc\n";
    const std::string_view expected =
        "org-data 0 24 0 24 0\n"
        "  section 0 24 0 24 0\n"
        "    paragraph 0 24 0 24 0\n"
        "      link 0 9 - - 1 type=id format=bracket path=a\n"
        "      link 9 18 - - 1 type=id format=bracket path=b\n"
        "      link 18 23 - - 0 type=id format=plain path=cc\n";
    CHECK(dump_of(source) == expected);
}

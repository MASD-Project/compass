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
#include "compass.platform/text/regex.hpp"

#include <stdexcept>
#include <string>
#include <catch2/catch_test_macros.hpp>

namespace {

using compass::platform::text::regex;
using compass::platform::text::regex_flags;

}

TEST_CASE("named groups and back references take Python's spelling",
    "[text][regex]") {
    const regex r(R"re(R"(?P<d>[^(\s]*)\(.*?\)(?P=d)")re",
        regex_flags{.dotall = true});
    const std::string text = "x R\"ab(one)\" two)ab\" y";
    const auto m = r.search(text);
    REQUIRE(m);
    CHECK(m->group("d") == "ab");
    CHECK(m->group() == "R\"ab(one)\" two)ab\"");
}

TEST_CASE("an unmatched group has no text", "[text][regex]") {
    const regex r(R"(namespace\s+(?P<n>\w+)?\s*\{)");
    const auto m = r.search("namespace {");
    REQUIRE(m);
    CHECK_FALSE(m->matched("n"));
    CHECK_FALSE(m->group_if("n"));
}

TEST_CASE("find_all does not repeat an empty match", "[text][regex]") {
    const regex r("x*");
    CHECK(r.find_all("axxb").size() == 4);
    CHECK(r.substitute("axxb", "-") == "-a--b-");
}

TEST_CASE("match is anchored at its start and search is not",
    "[text][regex]") {
    const regex r("b");
    CHECK_FALSE(r.match("ab"));
    CHECK(r.match("ab", 1));
    CHECK(r.search("ab"));
    CHECK_FALSE(regex("a").fullmatch("ab"));
}

TEST_CASE("word characters are Unicode letters, as in Python",
    "[text][regex]") {
    CHECK(regex(R"(^\w+$)").search("caf\xc3\xa9"));
    CHECK_FALSE(regex(R"(\bb)").search("\xc3\xa9" "b"));
}

TEST_CASE("an invalid byte matches nothing and does not fail",
    "[text][regex]") {
    const std::string text = "a\xff" "b";
    CHECK(regex("b").search(text)->start() == 2);
    CHECK_FALSE(regex("a.b").search(text));
}

TEST_CASE("a lazy loop over a large subject does not exhaust the stack",
    "[text][regex]") {
    const std::string text = "/*" + std::string(2'000'000, 'x') + "*/";
    const regex r(R"(/\*(?:.|\n)*?\*/)");
    CHECK(r.search(text)->end() == text.size());
}

TEST_CASE("a pattern that does not compile is refused", "[text][regex]") {
    CHECK_THROWS_AS(regex("(a"), std::invalid_argument);
}

TEST_CASE("escape quotes everything but word characters", "[text][regex]") {
    CHECK(compass::platform::text::escape("a::b.c") == R"(a\:\:b\.c)");
    CHECK(compass::platform::text::blank_keeping_newlines(
        "a\xc3\xa9\nb") == "  \n ");
}

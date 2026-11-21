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
#include "compass.env/ops/versions.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

namespace {

using namespace compass::env;
using Catch::Matchers::ContainsSubstring;

constexpr std::string_view document =
    "#+title: Check: env\n"
    "\n"
    "* Versions\n"
    "\n"
    "| version | date       | adds     | removes | format only | description |\n"
    "|---------+------------+----------+---------+-------------+-------------|\n"
    "| 1       | 2026-01-01 |          |         |             | Initial.    |\n"
    "\n"
    "* Variables\n"
    "\n"
    "| variable   | kind  | added | removed |\n"
    "|------------+-------+-------+---------|\n"
    "| =ACME_A=   | value | 1     |         |\n"
    "| ACME_NEW   | value |       |         |\n"
    "| ACME_OLD   | value | 1     |         |\n";

types::env_format format() {
    types::env_format f{.source = "env.org"};
    f.versions.push_back({.number = 1});
    return f;
}

}

TEST_CASE("a new version appends a dated row and sets the added version of "
    "its variables", "[versions]") {
    const auto text = ops::append_version(document, format(),
        {.description = "Adds new.", .adds = {"ACME_NEW"},
            .date = "2026-11-21"});
    REQUIRE(text);
    CHECK_THAT(*text, ContainsSubstring(
        "| 2       | 2026-11-21 | ACME_NEW | "));
    CHECK_THAT(*text, ContainsSubstring("| Adds new.   |\n"));
    CHECK_THAT(*text, ContainsSubstring(
        "| ACME_NEW   | value | 2     |         |\n"));
    CHECK_THAT(*text, ContainsSubstring(
        "| =ACME_A=   | value | 1     |         |\n"));
}

TEST_CASE("a cell too wide for its column aligns the table again",
    "[versions]") {
    const auto text = ops::append_version(document, format(),
        {.description = "A description longer than the column.",
            .adds = {"ACME_NEW"}, .date = "2026-11-21"});
    REQUIRE(text);
    CHECK_THAT(*text, ContainsSubstring(
        "| 1       | 2026-01-01 |          |         |             | "
        "Initial.                              |\n"));
    CHECK_THAT(*text, ContainsSubstring("|---------+------------+----------+"));
}

TEST_CASE("a version that removes sets the removed version", "[versions]") {
    const auto text = ops::append_version(document, format(),
        {.description = "Drops old.", .removes = {"ACME_OLD"},
            .date = "2026-11-21"});
    REQUIRE(text);
    CHECK_THAT(*text, ContainsSubstring(
        "| ACME_OLD   | value | 1     | 2       |\n"));
}

TEST_CASE("a version needs a variable, unless format only", "[versions]") {
    const auto none = ops::append_version(document, format(),
        {.description = "Nothing.", .date = "2026-11-21"});
    REQUIRE_FALSE(none);
    CHECK_THAT(none.error(), ContainsSubstring("--format-only"));
    const auto only = ops::append_version(document, format(),
        {.description = "Moved ports.", .format_only = true,
            .date = "2026-11-21"});
    REQUIRE(only);
    CHECK_THAT(*only, ContainsSubstring("| yes         | Moved ports."));
}

TEST_CASE("a version naming a variable no row has fails", "[versions]") {
    const auto text = ops::append_version(document, format(),
        {.description = "Ghost.", .adds = {"ACME_GHOST"},
            .date = "2026-11-21"});
    REQUIRE_FALSE(text);
    CHECK_THAT(text.error(), ContainsSubstring("ACME_GHOST"));
}

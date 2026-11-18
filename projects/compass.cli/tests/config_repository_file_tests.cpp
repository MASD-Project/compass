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
#include "compass.cli/config/repository_file.hpp"

#include <string>
#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

namespace {

using namespace compass::cli;
using Catch::Matchers::StartsWith;

constexpr std::string_view head =
    "#+title: Compass configuration\n"
    "#+output: out/compass\n"
    "#+meta: contracts\n"
    "\n"
    "* Codecs\n"
    "\n";

std::string error_of(std::string_view rows) {
    const auto r = config::parse_repository_file(
        std::string(head) + std::string(rows), ".compass.org");
    REQUIRE_FALSE(r);
    return r.error();
}

}

TEST_CASE("the keywords set the paths and each row sets enabled and its "
    "options, with the line it is on", "[config][repository_file]") {
    const auto r = config::parse_repository_file(std::string(head) +
        "| name | enabled | options |\n"
        "|------+---------+---------|\n"
        "| document_index | no | path=a.tsv  extra=x=y |\n"
        "| required_sections | yes | |\n",
        ".compass.org");
    REQUIRE(r);
    REQUIRE(r->output);
    CHECK(r->output->value == "out/compass");
    CHECK(r->output->from.kind == types::origin_kind::file);
    CHECK(r->output->from.line == 2);
    REQUIRE(r->meta);
    CHECK(r->meta->value == "contracts");
    CHECK(r->meta->from.line == 3);

    const auto& s = r->settings;
    REQUIRE(s.size() == 4);
    CHECK(s[0].codec == "document_index");
    CHECK(s[0].key == "enabled");
    CHECK(s[0].value == "false");
    CHECK(s[0].from.line == 9);
    CHECK(s[1].key == "path");
    CHECK(s[1].value == "a.tsv");
    CHECK(s[2].key == "extra");
    CHECK(s[2].value == "x=y");
    CHECK(s[3].codec == "required_sections");
    CHECK(s[3].key == "enabled");
    CHECK(s[3].value == "true");
    CHECK(s[3].from.line == 10);
}

TEST_CASE("a file without the path keywords leaves the paths unset",
    "[config][repository_file]") {
    const auto r = config::parse_repository_file(
        "* Codecs\n| document_index | yes | |\n", "f.org");
    REQUIRE(r);
    CHECK_FALSE(r->output);
    CHECK_FALSE(r->meta);
    CHECK(r->settings.size() == 1);
}

TEST_CASE("only a table under the level-1 heading Codecs counts",
    "[config][repository_file]") {
    const auto r = config::parse_repository_file(
        "* Other\n| a | yes | |\n** Codecs\n| b | yes | |\n", "f.org");
    REQUIRE_FALSE(r);
    CHECK(r.error() == "f.org: no table under a level-1 heading "
        "\"* Codecs\"; fix: add the heading and a | name | enabled | "
        "options | table under it");

    const auto empty = config::parse_repository_file("* Codecs\n", "f.org");
    REQUIRE_FALSE(empty);
    CHECK_THAT(empty.error(), StartsWith("f.org: no table under"));
}

TEST_CASE("each malformed row names the file, the line and the fix",
    "[config][repository_file]") {
    CHECK(error_of("| document_index | maybe | |\n") ==
        ".compass.org:7: enabled is 'maybe'; fix: write yes or no");
    CHECK(error_of("| document_index | yes |\n") ==
        ".compass.org:7: the row has 2 cells, not 3; fix: write | name | "
        "enabled | options |");
    CHECK(error_of("| document_index | yes | path |\n") ==
        ".compass.org:7: option 'path' is not key=value; fix: write each "
        "option as key=value, separated by spaces");
    CHECK(error_of("| document_index | yes | =x |\n") ==
        ".compass.org:7: option '=x' is not key=value; fix: write each "
        "option as key=value, separated by spaces");
    CHECK(error_of("| document_index | yes | enabled=no |\n") ==
        ".compass.org:7: enabled is set among the options; fix: use the "
        "enabled column");
    CHECK(error_of("| | yes | |\n") ==
        ".compass.org:7: the row names no codec; fix: write the codec's "
        "name in the first cell");
    CHECK(error_of("| document_index | yes | |\n| a | no | |\n"
                   "| document_index | no | |\n") ==
        ".compass.org:9: codec 'document_index' is configured twice, first "
        "on line 7; fix: keep one row per codec");
}

TEST_CASE("the table under Exceptions states one exception per row, and a "
    "row that does not hold is a malformed exception on its line",
    "[config][repository_file]") {
    const auto r = config::parse_repository_file(std::string(head) +
        "| document_index | yes | |\n"
        "* Exceptions\n"
        "| path | line | rule | expires | approver | reason |\n"
        "|------+------+------+---------+----------+--------|\n"
        "| =sql/a.sql= | 12 | ~sql_hygiene/not_re_runnable~ | 2026-12-31 "
        "| Marco C | made by =generate=, see a \\vert b |\n"
        "| doc/a.org | | required_sections/missing_section | | | |\n"
        "| doc/b.org | 3 | required_sections/missing_section |\n"
        "** Not this one\n"
        "| x.sql | 1 | not-a-rule | | | |\n",
        "abc1234:.compass.org");
    REQUIRE(r);
    const auto& s = r->exceptions;
    REQUIRE(s.exceptions.size() == 2);
    CHECK(s.exceptions[0].written_in == ".compass.org");
    CHECK(s.exceptions[0].line == 11);
    CHECK(s.exceptions[0].path == "sql/a.sql");
    CHECK(s.exceptions[0].covers == 12);
    CHECK(s.exceptions[0].rule == "sql_hygiene/not_re_runnable");
    CHECK(s.exceptions[0].approver == "Marco C");
    CHECK(s.exceptions[0].reason == "made by =generate=, see a \\vert b");
    CHECK(s.exceptions[1].path == "doc/a.org");
    CHECK(s.exceptions[1].covers == 0);
    CHECK(s.exceptions[1].reason.empty());
    CHECK_FALSE(s.exceptions[1].expires);
    REQUIRE(s.malformed.size() == 1);
    CHECK(s.malformed[0].written_in == ".compass.org");
    CHECK(s.malformed[0].line == 13);
    CHECK_THAT(s.malformed[0].why, StartsWith("the row has 3 cells, not 6"));
}

TEST_CASE("an empty path keyword is an error", "[config][repository_file]") {
    const auto r = config::parse_repository_file(
        "#+title: x\n#+output:\n* Codecs\n| a | yes | |\n", "f.org");
    REQUIRE_FALSE(r);
    CHECK(r.error() == "f.org:2: #+output: has no path; fix: write a path "
        "after #+output:, relative to the checkout");
}

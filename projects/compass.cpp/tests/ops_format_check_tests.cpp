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
#include "compass.cpp/ops/format_check.hpp"

#include <stdexcept>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

namespace {

using namespace compass::cpp;

constexpr std::string_view typescript =
    "SOURCE compass/checks/typescript_format.org\n"
    "FILES web/**/*.{ts,tsx}\n"
    "OUTSIDE web/**/node_modules/**\n"
    "FORMATTER prettier\n"
    "PROGRAM web/node_modules/.bin/prettier prettier\n"
    "STYLE .prettierrc.yaml\n";

std::string error_of(std::string_view text) {
    const auto d = ops::parse_format_definition(text);
    return d ? std::string() : d.error();
}

}

TEST_CASE("a format definition parses into its globs, formatter, program "
    "candidates and style", "[ops][format_check]") {
    const auto d = ops::parse_format_definition(typescript);
    REQUIRE(d);
    CHECK(d->source == "compass/checks/typescript_format.org");
    CHECK(d->files == std::vector<std::string>{"web/**/*.ts",
        "web/**/*.tsx"});
    CHECK(d->outside == std::vector<std::string>{
        "web/**/node_modules/**/*"});
    CHECK(d->formatter == "prettier");
    CHECK(d->programs == std::vector<std::string>{
        "web/node_modules/.bin/prettier", "prettier"});
    CHECK(d->style == ".prettierrc.yaml");
}

TEST_CASE("a format definition that cannot run says where and why",
    "[ops][format_check]") {
    CHECK(error_of("FORMATTER prettier\nPROGRAM p\nSTYLE s\n") ==
        "the definition has no FILES");
    CHECK(error_of("FILES *.ts\nPROGRAM p\nSTYLE s\n") ==
        "the definition has no FORMATTER");
    CHECK(error_of("FILES *.ts\nFORMATTER prettier\nSTYLE s\n") ==
        "the definition has no PROGRAM");
    CHECK(error_of("FILES *.ts\nFORMATTER prettier\nPROGRAM p\n") ==
        "the definition has no STYLE");
    CHECK(error_of("FILES *.ts\nFORMATTER black\n") ==
        "line 2: the formatter is 'black'; compass knows prettier and "
        "clang-format");
    CHECK(error_of("FILES *.ts\nFILES *.tsx\n") ==
        "line 2: FILES is given twice");
    CHECK(error_of("FILES *.ts\nCOLOUR red\n") == "line 2: unknown key "
        "COLOUR");
    CHECK(error_of("FILES *.ts\nRULE r\n") == "line 2: a format check has "
        "one rule, unformatted, and no examples; remove RULE");
    CHECK(error_of("FILES *.ts\nEXAMPLE quiet a.ts\n") == "line 2: a format "
        "check has one rule, unformatted, and no examples; remove EXAMPLE");
    CHECK(error_of("FILES a/{b.ts\n").starts_with("line 1: unclosed { in"));
}

TEST_CASE("a format check's formatter is the first of its candidates that "
    "is a program, or an error naming them all", "[ops][format_check]") {
    auto d = *ops::parse_format_definition(typescript);
    d.programs = {"no/such/prettier", "no-such-prettier-on-path"};
    CHECK_THROWS_WITH(ops::formatter_of(d, "/"),
        Catch::Matchers::ContainsSubstring("none of no/such/prettier, "
            "no-such-prettier-on-path is a program, for the formatter "
            "prettier"));
}

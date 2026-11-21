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
#include "compass.env/ops/format.hpp"

#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "acme_format.hpp"

namespace {

using namespace compass::env;
using Catch::Matchers::ContainsSubstring;

std::vector<std::uint32_t> rules_of(const tests::acme& a) {
    const auto f = ops::read_format(a.tables());
    REQUIRE(f);
    std::vector<std::uint32_t> out;
    for (const auto& finding : ops::check_format(*f))
        out.push_back(finding.rule);
    return out;
}

std::string unreadable(const tests::acme& a) {
    const auto f = ops::read_format(a.tables());
    REQUIRE_FALSE(f);
    CHECK(f.error().rule == ops::unreadable);
    return f.error().message;
}

tests::row new_variable(std::string added) {
    return {"ACME_NEW", "value", "x", std::move(added), "", "identity", "",
        "", "", "New."};
}

}

TEST_CASE("a consistent format reads and has no findings", "[format]") {
    const auto f = ops::read_format(tests::acme{}.tables());
    REQUIRE(f);
    CHECK(f->current() == 3);
    CHECK(f->version_variable == "ACME_ENV_VERSION");
    CHECK(f->variables.size() == 13);
    CHECK(f->versions[2].format_only);
    CHECK(rules_of({}).empty());
}

TEST_CASE("a variable row without a version is a finding", "[format]") {
    tests::acme a;
    a.variables.push_back(new_variable(""));
    CHECK(rules_of(a) == std::vector{ops::unknown_version});
}

TEST_CASE("a variable row that names an earlier version is a finding",
    "[format]") {
    tests::acme a;
    a.variables.push_back(new_variable("2"));
    CHECK(rules_of(a) == std::vector{ops::undeclared});
}

TEST_CASE("a variable row that names a version not in the table is a "
    "finding", "[format]") {
    tests::acme a;
    a.variables.push_back(new_variable("9"));
    CHECK(rules_of(a) == std::vector{ops::unknown_version});
}

TEST_CASE("a new version that declares the new row clears the finding",
    "[format]") {
    tests::acme a;
    a.variables.push_back(new_variable("4"));
    a.versions.push_back({"4", "2026-04-01", "ACME_NEW", "", "", "New."});
    CHECK(rules_of(a).empty());
}

TEST_CASE("a version after the baseline that declares nothing is a "
    "finding unless format only", "[format]") {
    tests::acme a;
    a.versions.push_back({"4", "2026-04-01", "", "", "", "Something."});
    CHECK(rules_of(a) == std::vector{ops::empty_version});
    a.versions.back()[4] = "yes";
    CHECK(rules_of(a).empty());
}

TEST_CASE("a version that lists a variable no row adds in it is a finding",
    "[format]") {
    tests::acme a;
    a.versions.push_back({"4", "2026-04-01", "ACME_GHOST", "", "", "G."});
    CHECK(rules_of(a) == std::vector{ops::unknown_variable});
}

TEST_CASE("the baseline version and those before it need declare nothing",
    "[format]") {
    tests::acme a;
    a.variables.push_back(new_variable("1"));
    CHECK(rules_of(a).empty());
}

TEST_CASE("unreadable formats name what does not read", "[format]") {
    tests::acme a;
    a.variables[1][1] = "guess";
    CHECK_THAT(unreadable(a), ContainsSubstring("guess"));

    a = {};
    a.versions[1][0] = "1";
    CHECK_THAT(unreadable(a), ContainsSubstring("versions rise"));

    a = {};
    a.variables[5][2] = "http://{ACME_LATER}";
    CHECK_THAT(unreadable(a), ContainsSubstring("ACME_LATER"));

    a = {};
    a.variables[2][2] = "{ACME_URL} 5";
    CHECK_THAT(unreadable(a), ContainsSubstring("a later row"));

    a = {};
    a.variables[1][5] = "nowhere";
    CHECK_THAT(unreadable(a), ContainsSubstring("nowhere"));

    a = {};
    a.settings.erase(a.settings.begin() + 1);
    CHECK_THAT(unreadable(a), ContainsSubstring("version_variable"));

    a = {};
    a.variables[1][2] = "{service.key}";
    CHECK_THAT(unreadable(a), ContainsSubstring("not per-service"));

    a = {};
    a.variables.back()[2] = "(unclosed";
    CHECK_THAT(unreadable(a), ContainsSubstring("does not compile"));
}

TEST_CASE("two rows of one name with conditions read without findings",
    "[format]") {
    CHECK(rules_of(tests::pooled_acme()).empty());
}

TEST_CASE("two rows of one name, one or both without a when, are a "
    "finding", "[format]") {
    auto a = tests::pooled_acme();
    a.variables[9][7] = "";
    CHECK(rules_of(a) == std::vector{ops::duplicate_variable});
    a.variables[8][7] = "";
    const auto f = ops::read_format(a.tables());
    REQUIRE(f);
    const auto findings = ops::check_format(*f);
    REQUIRE(findings.size() == 1);
    CHECK(findings[0].rule == ops::duplicate_variable);
    CHECK(findings[0].line == 29);
    CHECK_THAT(findings[0].message, ContainsSubstring("lines 28 and 29"));
    CHECK_THAT(findings[0].message, ContainsSubstring("neither has a when"));
}

TEST_CASE("two per-service rows of one name for no common service are no "
    "finding", "[format]") {
    auto a = tests::pooled_acme();
    const auto per_service = a.variables.size() - 4;
    a.variables[per_service][7] = "";
    a.variables[per_service][8] = "big";
    a.variables[per_service + 1][7] = "";
    a.variables[per_service + 1][8] = "!big";
    CHECK(rules_of(a).empty());
    a.variables[per_service + 1][8] = "";
    CHECK(rules_of(a) == std::vector{ops::duplicate_variable});
}

TEST_CASE("a comparing condition's names must be known when it is used",
    "[format]") {
    auto a = tests::pooled_acme();
    a.variables[8][7] = "{ACME_POOLER} != {ACME_GHOST}";
    CHECK_THAT(unreadable(a), ContainsSubstring("ACME_GHOST"));
    a = tests::pooled_acme();
    a.variables[8][7] = "{ACME_DB_URL} = 0";
    CHECK_THAT(unreadable(a), ContainsSubstring("a later row"));
    a = tests::pooled_acme();
    a.variables[7][7] = "{ACME_POOLER} != 0";
    CHECK(rules_of(a).empty());
}

TEST_CASE("a list cell splits at commas and blanks", "[format]") {
    CHECK(ops::list_of(" A, B  C,") ==
        std::vector<std::string>{"A", "B", "C"});
    CHECK(ops::list_of("").empty());
}

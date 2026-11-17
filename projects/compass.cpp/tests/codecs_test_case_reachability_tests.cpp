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
#include "compass.cpp/codecs/test_case_reachability.hpp"

#include <catch2/catch_test_macros.hpp>
#include "compass.cpp/ops/test_case_reachability.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

constexpr std::string_view guarded =
    "#include <catch2/catch_test_macros.hpp>\n"
    "TEST_CASE(\"open\", \"[a]\") {}\n"
    "#if defined(HAS_SOCKETS)\n"
    "TEST_CASE(\"hidden\", \"[a]\") {} TEST_CASE(\"twice\", \"[a]\") {}\n"
    "#  ifdef \\\n   DEEP\n"
    "SCENARIO_METHOD (fixture, \"deep\") {}\n"
    "#  endif\n"
    "#else\n"
    "// TEST_CASE(\"commented\") {}\n"
    "const char* s = \"TEST_CASE(\";\n"
    "#endif\n";

std::vector<std::uint32_t> lines_of(const ops::scan_result& r) {
    std::vector<std::uint32_t> out;
    for (const auto& c : r.cases)
        out.push_back(c.line);
    return out;
}

}

TEST_CASE("a case inside a conditional is found once per declaration, "
    "naming the innermost block", "[ops][test_case_reachability]") {
    const auto r = ops::scan_source(guarded);
    CHECK(r.declared == 4);
    CHECK(lines_of(r) == std::vector<std::uint32_t>{4, 4, 7});
    REQUIRE(r.cases.size() == 3);
    CHECK(r.cases[0].opened_line == 3);
    CHECK(r.cases[0].opened_text == "#if defined(HAS_SOCKETS)");
    CHECK(r.cases[2].opened_line == 5);
    CHECK(r.cases[2].opened_text == "#  ifdef \\");
    CHECK(r.problems.empty());
}

TEST_CASE("a file's own alias of a test macro declares cases",
    "[ops][test_case_reachability]") {
    const auto r = ops::scan_source(
        "#define MY_CASE(n) TEST_CASE(n, \"[x]\")\n"
        "#define LATER TEST_CASE_METHOD\n"
        "#define NOT_A_CASE TEST_CASES\n"
        "#ifdef X\nMY_CASE(\"a\") {}\nLATER(f, \"b\") {}\n"
        "NOT_A_CASE(\"c\") {}\nMY_CASEX(\"d\") {}\n#endif\n");
    CHECK(r.declared == 2);
    CHECK(lines_of(r) == std::vector<std::uint32_t>{5, 6});
}

TEST_CASE("unbalanced conditionals and a raw string splicing moves are "
    "problems", "[ops][test_case_reachability]") {
    const auto r = ops::scan_source(
        "#endif\n#ifndef A\n#if B\n#endif\n"
        "const char* s = R\"x(a)x\";\n"
        "const char* t = R\\\n\"y(#if 0)y\";\n");
    REQUIRE(r.problems.size() == 3);
    CHECK(r.problems[0].kind == ops::reading_problem::unopened_endif);
    CHECK(r.problems[0].line == 1);
    CHECK(r.problems[1].kind == ops::reading_problem::unclosed_conditional);
    CHECK(r.problems[1].line == 2);
    CHECK(r.problems[2].kind == ops::reading_problem::raw_string_moves);
    CHECK(r.problems[2].line == 6);
}

TEST_CASE("digit separators are not character literals",
    "[ops][test_case_reachability]") {
    const auto r = ops::scan_source(
        "int n = 1'000'000;\n#if X\nTEST_CASE(\"a\") {}\n#endif\n"
        "char c = '\"'; TEST_CASE(\"b\") {}\n");
    CHECK(r.declared == 2);
    CHECK(lines_of(r) == std::vector<std::uint32_t>{3});
    CHECK(ops::scan_source("x = 1'\"'; TEST_CASE(\"a\") {}\n").declared == 1);
}

TEST_CASE("test sources are under tests directories or named _tests, "
    "outside vendored trees", "[ops][test_case_reachability]") {
    CHECK(ops::is_test_source("projects/a/tests/x.cpp"));
    CHECK(ops::is_test_source("projects/a/tests/deep/x.INC"));
    CHECK(ops::is_test_source("projects/a/src/x_tests.hpp"));
    CHECK_FALSE(ops::is_test_source("projects/a/src/x.cpp"));
    CHECK_FALSE(ops::is_test_source("projects/a/tests/x.py"));
    CHECK_FALSE(ops::is_test_source("projects/a/build/tests/x.cpp"));
    CHECK_FALSE(ops::is_test_source("projects/external/x_tests.cpp"));
    CHECK_FALSE(ops::is_test_source("other/tests/x.cpp"));
    CHECK_FALSE(ops::is_test_source("projects/a/tests"));
}

TEST_CASE("the codec reports each listed case once and leaves the cases of "
    "an unreadable file out", "[codecs][test_case_reachability]") {
    const auto r = tests::run(codecs::test_case_reachability, {
        {"projects/a/tests/guarded_tests.cpp", std::string(guarded)},
        {"projects/a/tests/moved.cpp",
         "#if X\nTEST_CASE(\"a\") {}\n#endif\nauto s = R\\\n\"z()z\";\n"},
        {"projects/a/src/guarded.cpp", std::string(guarded)}});
    CHECK(tests::places(r.findings) == places{
        {"projects/a/tests/guarded_tests.cpp", 4},
        {"projects/a/tests/guarded_tests.cpp", 7},
        {"projects/a/tests/moved.cpp", 4}});
}

TEST_CASE("a tree whose test sources declare no case fails",
    "[codecs][test_case_reachability]") {
    const auto none = tests::run(codecs::test_case_reachability, {
        {"projects/a/tests/empty.cpp", "int main() {}\n"}});
    REQUIRE(none.findings.size() == 1);
    CHECK(std::get<0>(none.findings[0]) == "projects");
    const auto no_sources = tests::run(codecs::test_case_reachability, {
        {"projects/a/src/a.cpp", "TEST_CASE(\"x\") {}\n"}});
    CHECK(no_sources.findings.size() == 1);
}

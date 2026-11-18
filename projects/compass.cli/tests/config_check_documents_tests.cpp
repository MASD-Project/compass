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
#include "compass.cli/config/check_documents.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::cli;

constexpr std::string_view document =
    "#+title: Check: portable C++ facilities\n"     // 1
    "#+description: No code uses a facility a platform lacks.\n"
    "\n"
    "* Definition\n"                                 // 4
    ":PROPERTIES:\n"
    ":ENGINE: forbid\n"
    ":FILES: src/**/*.{cpp,hpp}\n"                   // 7
    ":OUTSIDE: src/platform/**\n"
    ":END:\n"
    "\n"
    "The settings every rule inherits.\n"
    "\n"
    "** stop_types\n"                                // 13
    ":PROPERTIES:\n"
    ":CPP_NAME: std::jthread\n"
    ":MESSAGE: {match} is not portable\n"
    ":END:\n"
    "\n"
    "Apple's libc++ has no\n"
    "C++20 stop types.\n"
    "\n"
    "** getenv\n"                                    // 22
    ":PROPERTIES:\n"
    ":CPP_NAME: getenv\n"
    ":MESSAGE: {match} is deprecated\n"
    ":FIX: use the platform's environment\n"
    ":END:\n"
    "\n"
    "* Exceptions\n"                                 // 29
    "\n"
    "| path | line | rule | expires | approver | reason |\n"
    "|------+------+------+---------+----------+--------|\n"
    "| src/a.cpp | 3 | getenv | 2027-01-31 | marco | read once at start |\n"
    "| src/b.cpp | | stop_types | 2027-01-31 | marco |\n";

std::string error_of(std::string_view text) {
    compass::codec::types::exception_set e;
    const auto c = config::parse_check_document(text,
        "compass/checks/portable_cpp.org", e);
    return c ? std::string("parsed") : c.error();
}

std::string replaced(std::string_view text, std::string_view from,
    std::string_view to) {
    std::string s(text);
    s.replace(s.find(from), from.size(), to);
    return s;
}

}

TEST_CASE("a check document becomes a codec named for its file, with a "
    "rule per heading under its definition", "[config][check_documents]") {
    compass::codec::types::exception_set e;
    const auto c = config::parse_check_document(document,
        "compass/checks/portable_cpp.org", e);
    REQUIRE(c);
    const auto& codec = (*c)->codec;
    CHECK(codec.name == "portable_cpp");
    CHECK(codec.summary == "No code uses a facility a platform lacks.");
    CHECK(codec.enabled_by_default);
    REQUIRE(codec.rules.size() == 2);
    CHECK(codec.rules[0].name == "stop_types");
    CHECK(codec.rules[0].summary ==
        "Apple's libc++ has no C++20 stop types.");
    CHECK(codec.rules[1].name == "getenv");
    CHECK(codec.rules[1].summary == "{match} is deprecated");
    REQUIRE(codec.options.size() == 1);
    CHECK(codec.options[0].key == "definition");
    CHECK(codec.options[0].default_value ==
        "FILES src/**/*.{cpp,hpp}\n"
        "OUTSIDE src/platform/**\n"
        "RULE stop_types\n"
        "CPP_NAME std::jthread\n"
        "MESSAGE {match} is not portable\n"
        "RULE getenv\n"
        "CPP_NAME getenv\n"
        "MESSAGE {match} is deprecated\n"
        "FIX use the platform's environment\n");
}

TEST_CASE("the exceptions of a check name its rules without the check",
    "[config][check_documents]") {
    compass::codec::types::exception_set e;
    REQUIRE(config::parse_check_document(document,
        "compass/checks/portable_cpp.org", e));
    REQUIRE(e.exceptions.size() == 1);
    CHECK(e.exceptions[0].rule == "portable_cpp/getenv");
    CHECK(e.exceptions[0].path == "src/a.cpp");
    CHECK(e.exceptions[0].covers == 3);
    CHECK(e.exceptions[0].written_in == "compass/checks/portable_cpp.org");
    CHECK(e.exceptions[0].line == 33);
    REQUIRE(e.malformed.size() == 1);
    CHECK(e.malformed[0].line == 34);
}

TEST_CASE("prose is not part of the definition, so editing it changes no "
    "cache key", "[config][check_documents]") {
    compass::codec::types::exception_set e;
    const auto before = config::parse_check_document(document, "a.org", e);
    const auto after = config::parse_check_document(replaced(document,
        "C++20 stop types.", "any of the C++20 stop types."), "a.org", e);
    REQUIRE(before);
    REQUIRE(after);
    CHECK((*before)->definition == (*after)->definition);
    CHECK((*after)->codec.rules[0].summary ==
        "Apple's libc++ has no any of the C++20 stop types.");
}

TEST_CASE("a check document that cannot run says where and why",
    "[config][check_documents]") {
    CHECK(error_of("#+title: x\n\n* Rule\n") ==
        "compass/checks/portable_cpp.org:1: the document has no level-1 "
        "heading \"* Definition\"; fix: add it, with :ENGINE: forbid and the "
        "check's FILES in its property drawer");
    CHECK(error_of(replaced(document, ":ENGINE: forbid\n", "")) ==
        "compass/checks/portable_cpp.org:4: the definition names no engine; "
        "fix: add :ENGINE: forbid to its property drawer, which org reads "
        "only on the line right under the heading");
    CHECK(error_of(replaced(document, ":ENGINE: forbid", ":ENGINE: limit")) ==
        "compass/checks/portable_cpp.org:6: the engine is 'limit'; fix: "
        "write :ENGINE: forbid, the one engine compass runs from a document");
    CHECK(error_of(replaced(document, ":CPP_NAME: getenv", ":COLOUR: red")) ==
        "compass/checks/portable_cpp.org:24: unknown key COLOUR; fix: "
        "correct the definition's drawers");
    CHECK(error_of(replaced(document, ":MESSAGE: {match} is not portable\n",
        "")) == "compass/checks/portable_cpp.org:13: rule stop_types has no "
        "MESSAGE; fix: correct the definition's drawers");
    CHECK(error_of(replaced(document, ":FILES: src/**/*.{cpp,hpp}\n", "")) ==
        "compass/checks/portable_cpp.org:4: the definition has no FILES; "
        "fix: correct the definition's drawers");
}

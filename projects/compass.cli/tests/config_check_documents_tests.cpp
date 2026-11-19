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
    "* Examples\n"                                   // 29
    "\n"
    "#+begin_src cpp :compass fires=getenv :path src/a.cpp\n"   // 31
    "auto p = getenv(\"P\");\n"
    "\n"
    ",#+include it\n"
    "#+end_src\n"
    "\n"
    "#+begin_src cpp :path src/b.cpp\n"
    "not an example\n"
    "#+end_src\n"
    "\n"
    "* Exceptions\n"                                 // 41
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
    REQUIRE(codec.rules.size() == 3);
    CHECK(codec.rules[0].name == "stop_types");
    CHECK(codec.rules[0].summary ==
        "Apple's libc++ has no C++20 stop types.");
    CHECK(codec.rules[1].name == "getenv");
    CHECK(codec.rules[1].summary == "{match} is deprecated");
    CHECK(codec.rules[2].name == "example");
    REQUIRE(codec.options.size() == 1);
    CHECK(codec.options[0].key == "definition");
    CHECK(codec.options[0].default_value ==
        "SOURCE compass/checks/portable_cpp.org\n"
        "FILES src/**/*.{cpp,hpp}\n"
        "OUTSIDE src/platform/**\n"
        "RULE stop_types\n"
        "CPP_NAME std::jthread\n"
        "MESSAGE {match} is not portable\n"
        "RULE getenv\n"
        "CPP_NAME getenv\n"
        "MESSAGE {match} is deprecated\n"
        "FIX use the platform's environment\n"
        "EXAMPLE fires=getenv src/a.cpp\n"
        "> auto p = getenv(\"P\");\n"
        ">\n"
        "> #+include it\n");
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
    CHECK(e.exceptions[0].line == 45);
    REQUIRE(e.malformed.size() == 1);
    CHECK(e.malformed[0].line == 46);
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
        "fix: add :ENGINE: forbid or :ENGINE: format to its property drawer, "
        "which org reads only on the line right under the heading");
    CHECK(error_of(replaced(document, ":ENGINE: forbid", ":ENGINE: limit")) ==
        "compass/checks/portable_cpp.org:6: the engine is 'limit'; fix: "
        "write :ENGINE: forbid or :ENGINE: format, the engines compass runs "
        "from a document");
    CHECK(error_of(replaced(document, ":CPP_NAME: getenv", ":COLOUR: red")) ==
        "compass/checks/portable_cpp.org:24: unknown key COLOUR; fix: "
        "correct the definition's drawers");
    CHECK(error_of(replaced(document, ":MESSAGE: {match} is not portable\n",
        "")) == "compass/checks/portable_cpp.org:13: rule stop_types has no "
        "MESSAGE; fix: correct the definition's drawers");
    CHECK(error_of(replaced(document, "fires=getenv", "defines")) ==
        "compass/checks/portable_cpp.org:31: a forbid check has no "
        "definitions to give; fix: mark the block fires=<rule> or quiet");
    CHECK(error_of(replaced(document, " :path src/a.cpp", "")) ==
        "compass/checks/portable_cpp.org:31: the example has no :path; fix: "
        "add :path <path> to its header, the file it stands for");
    CHECK(error_of(replaced(document, "fires=getenv", "fires=setenv")) ==
        "compass/checks/portable_cpp.org:31: the example fires setenv, which "
        "is no rule of the definition; fix: correct the definition's "
        "drawers");
    CHECK(error_of(replaced(document, ":FILES: src/**/*.{cpp,hpp}\n", "")) ==
        "compass/checks/portable_cpp.org:4: the definition has no FILES; "
        "fix: correct the definition's drawers");
}

TEST_CASE("a format check document becomes a formatting check with one "
    "rule, unformatted", "[config][check_documents]") {
    const std::string format_document =
        "#+title: Check: TypeScript formatting\n"
        "#+description: The TypeScript is as prettier formats it.\n\n"
        "* Definition\n"
        ":PROPERTIES:\n"
        ":ENGINE: format\n"
        ":FORMATTER: prettier\n"
        ":PROGRAM: prettier\n"
        ":STYLE: .prettierrc.yaml\n"
        ":FILES: web/**/*.ts\n"
        ":END:\n";
    compass::codec::types::exception_set exceptions;
    const auto c = config::parse_check_document(format_document,
        "compass/checks/typescript_format.org", exceptions);
    REQUIRE(c);
    const auto& codec = (*c)->codec;
    CHECK(codec.name == "typescript_format");
    CHECK(codec.formatting_check);
    CHECK(codec.formatters != nullptr);
    REQUIRE(codec.rules.size() == 1);
    CHECK(codec.rules[0].name == "unformatted");
    CHECK(codec.rules[0].summary ==
        "Each file the check covers is as prettier formats it.");

    compass::codec::types::exception_set ignored;
    const auto headed = config::parse_check_document(format_document +
        "\n** typescript\n:PROPERTIES:\n:MESSAGE: m\n:END:\n",
        "compass/checks/typescript_format.org", ignored);
    REQUIRE_FALSE(headed);
    CHECK(headed.error() == "compass/checks/typescript_format.org:13: a "
        "format check has one rule, unformatted, and no rule headings; fix: "
        "remove the heading, and state the check in the Definition drawer");
}

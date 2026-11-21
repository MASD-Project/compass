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
#include "compass.codec/ops/data_tables.hpp"

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
        "fix: add :ENGINE: forbid, format or limit to its property drawer, "
        "which org reads only on the line right under the heading");
    CHECK(error_of(replaced(document, ":ENGINE: forbid", ":ENGINE: count")) ==
        "compass/checks/portable_cpp.org:6: the engine is 'count'; fix: "
        "write :ENGINE: forbid, format or limit, the engines compass runs "
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
        "format check has fixed rules and no rule headings; fix: "
        "remove the heading, and state the check in the Definition drawer");
}

TEST_CASE("a limit check document records each row of its Ceilings in its "
    "definition", "[config][check_documents]") {
    const std::string limit_document =
        "#+title: Check: wide types\n"
        "#+description: No type holds more than fifteen fields.\n\n"
        "* Definition\n"
        ":PROPERTIES:\n"
        ":ENGINE: limit\n"
        ":FILES: src/**/*.hpp\n"
        ":FACT: cpp_fields\n"
        ":CEILING: 15\n"
        ":END:\n\n"
        "* Ceilings\n\n"
        "| path | type | ceiling |\n"
        "|------+------+---------|\n"
        "| =src/a.hpp= | trade | 40 |\n";                 // 15
    compass::codec::types::exception_set exceptions;
    const auto c = config::parse_check_document(limit_document,
        "compass/checks/wide_types.org", exceptions);
    REQUIRE(c);
    const auto& codec = (*c)->codec;
    REQUIRE(codec.rules.size() == 2);
    CHECK(codec.rules[0].name == "over_ceiling");
    CHECK(codec.rules[1].name == "below_record");
    CHECK(codec.options[0].default_value ==
        "SOURCE compass/checks/wide_types.org\n"
        "FILES src/**/*.hpp\n"
        "FACT cpp_fields\n"
        "CEILING 15\n"
        "RECORDED src/a.hpp trade 40\n");

    compass::codec::types::exception_set ignored;
    const auto low = config::parse_check_document(replaced(limit_document,
        "| trade | 40 |", "| trade | 9 |"), "compass/checks/wide_types.org",
        ignored);
    REQUIRE_FALSE(low);
    CHECK(low.error() == "compass/checks/wide_types.org:4: trade of "
        "src/a.hpp is recorded at 9, not above the ceiling of 15; fix: "
        "correct the definition's drawers");
}

TEST_CASE("a check document's SEVERITY sets the severity of its findings",
    "[config][check_documents]") {
    compass::codec::types::exception_set exceptions;
    const auto plain = config::parse_check_document(document,
        "compass/checks/portable_cpp.org", exceptions);
    REQUIRE(plain);
    CHECK((*plain)->codec.level == compass::codec::types::severity::error);

    compass::codec::types::exception_set ignored;
    const auto warned = config::parse_check_document(replaced(document,
        ":ENGINE: forbid\n", ":ENGINE: forbid\n:SEVERITY: warning\n"),
        "compass/checks/portable_cpp.org", ignored);
    REQUIRE(warned);
    CHECK((*warned)->codec.level == compass::codec::types::severity::warning);
    CHECK((*warned)->codec.options[0].default_value.find("SEVERITY") ==
        std::string_view::npos);

    CHECK(error_of(replaced(document, ":ENGINE: forbid\n",
        ":ENGINE: forbid\n:SEVERITY: loud\n")) ==
        "compass/checks/portable_cpp.org:7: the severity is 'loud'; fix: "
        "write :SEVERITY: error, which fails a check that finds anything, "
        "or warning, which reports and fails nothing");
}

namespace {

constexpr std::string_view codec_document =
    "#+title: Check: conventions coverage\n"             // 1
    "\n"
    "Prose the data does not hold.\n"
    "\n"
    "* Definition\n"                                     // 5
    ":PROPERTIES:\n"
    ":CODEC: conventions_coverage\n"                     // 7
    ":END:\n"
    "\n"
    "* Index fields\n"                                   // 10
    "\n"
    "The fields that name an index.\n"
    "\n"
    "| field |\n"
    "|-------|\n"
    "| =Index= |\n"                                      // 16
    "| FlatIndex |\n"
    "\n"
    "* Patterns\n"                                       // 19
    "\n"
    "| name | pattern | note |\n"
    "|------+---------+------|\n"
    "| either | a\\vert{}b | |\n"                          // 23
    "\n"
    "* Exceptions\n"
    "\n"
    "| path | line | rule | expires | approver | reason |\n"
    "| x.tsv | 1 | no_source | 2027-01-31 | marco | kept for the demo |\n";

}

TEST_CASE("a document whose Definition names a codec gives it tables, not "
    "a check", "[config][check_documents]") {
    compass::codec::types::exception_set e;
    const auto d = config::parse_codec_document(codec_document,
        "compass/checks/conventions_coverage.org", e);
    REQUIRE(d);
    REQUIRE(*d);
    CHECK((*d)->codecs == std::vector<std::string>{"conventions_coverage"});
    CHECK((*d)->line == 7);
    const auto t = compass::codec::ops::decode_data_tables((*d)->text);
    REQUIRE(t);
    CHECK(t->source == "compass/checks/conventions_coverage.org");
    REQUIRE(t->tables.size() == 2);
    const auto& fields = t->tables[0];
    CHECK(fields.name == "Index fields");
    CHECK(fields.line == 10);
    CHECK(fields.columns == std::vector<std::string>{"field"});
    CHECK(fields.rows == std::vector<std::vector<std::string>>{
        {"Index"}, {"FlatIndex"}});
    CHECK(fields.row_lines == std::vector<std::uint32_t>{16, 17});
    const auto& patterns = *t->find("patterns");
    CHECK(patterns.rows == std::vector<std::vector<std::string>>{
        {"either", "a|b", ""}});
    CHECK(patterns.row_lines == std::vector<std::uint32_t>{23});
    CHECK(e.exceptions.size() == 1);
}

TEST_CASE("a codec's tables hold no prose, so editing prose leaves them as "
    "they were", "[config][check_documents]") {
    compass::codec::types::exception_set e;
    const auto before = config::parse_codec_document(codec_document,
        "compass/checks/conventions_coverage.org", e);
    const auto after = config::parse_codec_document(replaced(codec_document,
        "The fields that name an index.", "The fields an index is named by."),
        "compass/checks/conventions_coverage.org", e);
    REQUIRE(before);
    REQUIRE(after);
    CHECK((*before)->text == (*after)->text);
}

TEST_CASE("a document gives one copy of its tables to several codecs, "
    "whose exceptions name the codec", "[config][check_documents]") {
    compass::codec::types::exception_set e;
    auto text = replaced(codec_document, ":CODEC: conventions_coverage\n",
        ":CODEC: ore_value_sets  parameter_value_sets\n");
    text = replaced(text, "| x.tsv | 1 | no_source |",
        "| x.tsv | 1 | parameter_value_sets/clash |");
    text += "| y.tsv | 2 | clash | 2027-01-31 | marco | no codec named |\n";
    const auto d = config::parse_codec_document(text,
        "compass/checks/value_sets.org", e);
    REQUIRE(d);
    REQUIRE(*d);
    CHECK((*d)->codecs == std::vector<std::string>{"ore_value_sets",
        "parameter_value_sets"});
    REQUIRE(e.exceptions.size() == 1);
    CHECK(e.exceptions[0].rule == "parameter_value_sets/clash");
    REQUIRE(e.malformed.size() == 1);
    CHECK(e.malformed[0].line == 29);
    CHECK(e.malformed[0].why.starts_with("the rule clash names none of "
        "the codecs the document gives data to"));
}

TEST_CASE("a codec document holds only its codec's name in its drawer, and "
    "a check document is not one", "[config][check_documents]") {
    compass::codec::types::exception_set e;
    const auto extra = config::parse_codec_document(replaced(codec_document,
        ":CODEC: conventions_coverage\n",
        ":CODEC: conventions_coverage\n:FILES: x\n"),
        "compass/checks/conventions_coverage.org", e);
    REQUIRE_FALSE(extra);
    CHECK(extra.error().starts_with(
        "compass/checks/conventions_coverage.org:8: a codec's data names "
        "its codec and nothing else, and the drawer also holds FILES"));
    const auto check = config::parse_codec_document(document,
        "compass/checks/portable_cpp.org", e);
    REQUIRE(check);
    CHECK_FALSE(*check);
}

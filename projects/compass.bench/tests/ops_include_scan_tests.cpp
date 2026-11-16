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
#include "compass.bench/ops/include_scan.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::bench;

struct found {
    std::uint32_t line;
    types::include_kind kind;
    std::string name;
    bool operator==(const found&) const = default;
};

std::vector<found> scan(std::string_view text) {
    types::include_table t;
    ops::scan_includes(text, 7, t);
    std::vector<found> r;
    for (std::size_t i = 0; i < t.size(); ++i) {
        CHECK(t.includers[i] == 7);
        r.push_back({t.lines[i], t.kinds[i], std::string(t.name(i))});
    }
    return r;
}

constexpr auto quoted = types::include_kind::quoted;
constexpr auto angle = types::include_kind::angle;

}

TEST_CASE("scan_includes finds both forms with blanks around the hash",
    "[ops][include_scan]") {
    const auto r = scan(
        "#include \"a.hpp\"\n"
        "   #include <b/c.hpp>\n"
        "\t#  include\t\"d.h\" // trailing\n"
        "int x;\n"
        "#include<e>\n");

    const std::vector<found> expected = {{1, quoted, "a.hpp"},
        {2, angle, "b/c.hpp"}, {3, quoted, "d.h"}, {5, angle, "e"}};
    CHECK(r == expected);
}

TEST_CASE("scan_includes ignores includes inside comments",
    "[ops][include_scan]") {
    const auto r = scan(
        "// #include \"line_comment.hpp\"\n"
        "/* first line\n"
        "#include \"block_comment.hpp\"\n"
        "*/\n"
        "/* x */ #include \"after_comment.hpp\"\n"
        "// continued \\\n"
        "#include \"continued_comment.hpp\"\n"
        "#include \"real.hpp\"\n");

    const std::vector<found> expected = {
        {5, quoted, "after_comment.hpp"}, {8, quoted, "real.hpp"}};
    CHECK(r == expected);
}

TEST_CASE("scan_includes ignores includes inside string literals",
    "[ops][include_scan]") {
    const auto r = scan(
        "const char* s = \"#include <string_literal>\";\n"
        "const char q = '\"'; const char* t = \"/*\";\n"
        "#include \"after_quote_char.hpp\"\n"
        "auto raw = R\"x(\n"
        "#include \"raw_string.hpp\"\n"
        ")x\";\n"
        "int n = 1'000'000;\n"
        "#include \"after_raw.hpp\"\n");

    const std::vector<found> expected = {
        {3, quoted, "after_quote_char.hpp"}, {8, quoted, "after_raw.hpp"}};
    CHECK(r == expected);
}

TEST_CASE("scan_includes skips other directives and macro operands",
    "[ops][include_scan]") {
    types::include_table t;
    ops::scan_includes(
        "#include_next <a>\n"
        "#define X <b>\n"
        "int y; #include \"not_first_token.hpp\"\n"
        "#include BOOST_PP_ITERATE()\n"
        "#include \"unterminated.hpp\n"
        "#include \"last.hpp\"",
        0, t);

    REQUIRE(t.size() == 1);
    CHECK(t.name(0) == "last.hpp");
    CHECK(t.lines[0] == 6);
    CHECK(t.computed == 1);
}

TEST_CASE("scan_includes counts lines across CRLF endings",
    "[ops][include_scan]") {
    const auto r = scan(
        "#include \"a.hpp\"\r\n"
        "\r\n"
        "  /* one\r\n two */\r\n"
        "#include <b.hpp>\r\n"
        "// continued \\\r\n"
        "#include \"continued_comment.hpp\"\r\n"
        "#include \"c.hpp\"\r\n");

    const std::vector<found> expected = {
        {1, quoted, "a.hpp"}, {5, angle, "b.hpp"}, {8, quoted, "c.hpp"}};
    CHECK(r == expected);
}

TEST_CASE("is_scanned_extension accepts only the C and C++ extensions",
    "[ops][include_scan]") {
    for (const auto e : {".hpp", ".cpp", ".h", ".hh", ".cc", ".cxx", ".ipp",
             ".tpp"})
        CHECK(ops::is_scanned_extension(e));
    for (const auto e : {"", ".c", ".hxx", ".txt", ".HPP", "hpp"})
        CHECK_FALSE(ops::is_scanned_extension(e));
}

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
#include "compass.cpp/codecs/enum_attributes.hpp"

#include <catch2/catch_test_macros.hpp>
#include "compass.cpp/ops/enum_attributes.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;
using macro_set = std::set<std::string, std::less<>>;

files tree() {
    return {
        {"projects/ores.a/include/ores.a/export.hpp",
         "#ifdef ORES_A_LIBRARY\n"
         "#define ORES_A_EXPORT \\\n    BOOST_SYMBOL_EXPORT\n"
         "#endif\n"},
        {"projects/ores.a/include/ores.a/kinds.hpp",
         "#include \"ores.a/export.hpp\"\n"
         "enum class ORES_A_EXPORT colour : int { red };\n"
         "// enum class ORES_A_EXPORT gone : int {};\n"
         "const char* s = \"enum ORES_A_EXPORT quoted {\";\n"
         "enum ORES_A_EXPORT plain;\n"
         "enum class fine : int {};\n"},
        {"projects/ores.a/src/kinds.cpp",
         "using enum ORES_A_EXPORT colour;\n"},
        {"projects/ores.a/src/notes.txt",
         "enum class ORES_A_EXPORT ignored {};\n"},
        {"other/kinds.hpp", "enum class BOOST_SYMBOL_EXPORT x {};\n"}};
}

}

TEST_CASE("a visibility macro another file defines is reported on the "
    "enums it decorates", "[codecs][enum_attributes]") {
    const auto r = tests::run(codecs::enum_attributes, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.a/include/ores.a/kinds.hpp", 2},
        {"projects/ores.a/include/ores.a/kinds.hpp", 5}});
    REQUIRE(r.findings.size() == 2);
    CHECK(std::get<2>(r.findings[0]) == "the visibility macro ORES_A_EXPORT "
        "decorates an enum, and the attribute does not apply to one: "
        "enum class ORES_A_EXPORT colour : int { red };");
}

TEST_CASE("an edit to the file defining a macro changes the findings on "
    "the files it decorates", "[codecs][enum_attributes]") {
    const auto export_header = "projects/ores.a/include/ores.a/export.hpp";
    auto after = tree();
    after[export_header] = "#define ORES_A_EXPORT\n";

    CHECK(tests::run_after_edit(codecs::enum_attributes, tree(), after)
        .findings.empty());
    CHECK(tests::places(tests::run_after_edit(codecs::enum_attributes,
        after, tree()).findings) == places{
            {"projects/ores.a/include/ores.a/kinds.hpp", 2},
            {"projects/ores.a/include/ores.a/kinds.hpp", 5}});
}

TEST_CASE("a macro whose body is not a visibility attribute is not "
    "reported", "[codecs][enum_attributes]") {
    auto all = tree();
    all["projects/ores.a/include/ores.a/export.hpp"] =
        "#define ORES_A_EXPORT [[nodiscard]]\n";
    CHECK(tests::run(codecs::enum_attributes, all).findings.empty());
}

TEST_CASE("Boost's own macros are visibility macros without a definition",
    "[codecs][enum_attributes]") {
    const auto r = tests::run(codecs::enum_attributes, {
        {"projects/x.cc", "enum struct BOOST_SYMBOL_IMPORT e { a };\n"}});
    CHECK(tests::places(r.findings) == places{{"projects/x.cc", 1}});
}

TEST_CASE("erasing blanks comments and literals byte for byte and keeps "
    "newlines", "[ops][enum_attributes]") {
    CHECK(ops::erase_comments_and_literals("a // b\nc") == "a     \nc");
    CHECK(ops::erase_comments_and_literals("a /* b\n */c") ==
        "a     \n   c");
    CHECK(ops::erase_comments_and_literals("x \"a\\\"b\" y") ==
        "x        y");
    CHECK(ops::erase_comments_and_literals("'a\nb' c") == "  \n   c");
    CHECK(ops::erase_comments_and_literals("/*/ x") == "    x");
}

TEST_CASE("a digit separator does not open a character literal",
    "[ops][enum_attributes]") {
    CHECK(ops::erase_comments_and_literals("int n = 1'000;\nenum class e;") ==
        "int n = 1'000;\nenum class e;");
    CHECK(ops::erase_comments_and_literals("x = 0xFF'FFu + 0b1'0;\n'a'") ==
        "x = 0xFF'FFu + 0b1'0;\n   ");
    CHECK(ops::erase_comments_and_literals("u8'a' L'b'") == "u8    L   ");
}

TEST_CASE("a visibility macro is a define whose body is a visibility or "
    "DLL attribute", "[ops][enum_attributes]") {
    macro_set m;
    ops::add_visibility_macros(
        "#define A __declspec ( dllexport )\n"
        " # define B __attribute__ ((visibility(\"default\")))\n"
        "#define C(x) BOOST_SYMBOL_EXPORT\n"
        "#define D \\\nBOOST_SYMBOL_IMPORT\n"
        "#define E __declspec(noinline)\n"
        "x #define F BOOST_SYMBOL_EXPORT\n", m);
    CHECK(m == macro_set{"A", "B", "D"});
}

TEST_CASE("an enum site needs a declaration tail and is not one after "
    "using", "[ops][enum_attributes]") {
    const macro_set m{"M"};
    const auto sites = ops::enum_sites(
        "enum class M e : int {};\n"
        "void f(enum E e);\n"
        "using enum M e;\n"
        "enum M { a };\n"
        "enum\tstruct\nN;\n"
        "xenum M y;\n", m);
    REQUIRE(sites.size() == 2);
    CHECK(sites[0].offset == 0);
    CHECK(sites[0].macro == "M");
    CHECK(sites[0].source == "enum class M e : int {};");
    CHECK(sites[1].macro.empty());
    CHECK(sites[1].source == "enum\tstruct");
}

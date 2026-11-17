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
#include "compass.cpp/ops/lexer.hpp"

#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.cpp/ops/source_model.hpp"

using namespace compass::cpp::ops;

namespace {

/**
 * @brief The code of @p source with runs of blanks squeezed to one, so a
 * test states what survives lexing.
 */
std::string survivors(std::string_view source) {
    const auto l = lex(source);
    std::string out;
    for (const char c : l.code) {
        if (c == ' ' && !out.empty() && out.back() == ' ')
            continue;
        out.push_back(c);
    }
    return out;
}

std::uint32_t at(std::string_view source, std::string_view word) {
    return static_cast<std::uint32_t>(source.find(word));
}

}

TEST_CASE("the code keeps every offset of the source", "[ops][lexer]") {
    const std::string_view s = "a /* b */ \"c\" 'd' // e\nf";
    const auto l = lex(s);
    CHECK(l.code.size() == s.size());
    CHECK(l.code == "a                     \nf");
}

TEST_CASE("a raw string ends at its own delimiter", "[ops][lexer]") {
    CHECK(survivors("x = R\"d(a )\" // b)d\"; int y;") == "x = ; int y;");
    CHECK(survivors("x = u8R\"(/* )\"; int y;") == "x = ; int y;");
    CHECK(lex("x = R\"(\n\"\n)\"; int y;").code ==
        "x =    \n \n  ; int y;");
}

TEST_CASE("a quote in a number separates digits", "[ops][lexer]") {
    CHECK(survivors("int n = 1'000'000; char c = 'x'; int m;") ==
        "int n = 1'000'000; char c = ; int m;");
    CHECK(survivors("x = 0xFF'FFu + 1.5e+3'0; '\"' y") ==
        "x = 0xFF'FFu + 1.5e+3'0; y");
    const auto l = lex("int n = 1'0; char c = 'x';");
    REQUIRE(l.literals.size() == 1);
    CHECK(l.literals[0].begin == 22);
}

TEST_CASE("a splice continues a comment and a directive", "[ops][lexer]") {
    const std::string_view s =
        "// a \\\nint hidden;\n#define A \\\n B\nint c;";
    const auto l = lex(s);
    CHECK(l.code.find("hidden") == std::string::npos);
    CHECK(l.code.find("int c;") != std::string::npos);
    REQUIRE(l.splices.size() == 2);
    REQUIRE(l.directives.size() == 1);
    CHECK(std::string_view(l.code).substr(l.directives[0].begin,
        l.directives[0].end - l.directives[0].begin) == "#define A  \n B");
}

TEST_CASE("comments and literals do not open one another", "[ops][lexer]") {
    CHECK(survivors("/* \"x */ a \"/*\" b // c") == " a b ");
    CHECK(survivors("a \"x\\\"y\" b") == "a b");
    CHECK(survivors("don't = 1;\nx") == "don't = 1;\nx");
}

TEST_CASE("an unterminated string ends with its line", "[ops][lexer]") {
    CHECK(survivors("a \"b\nc") == "a \nc");
}

TEST_CASE("an #if 0 branch is dead and an #if 1 branch excludes the rest",
    "[ops][lexer]") {
    const std::string_view s = "#if 0\nint d1;\n#elif X\nint l1;\n#endif\n"
        "#if 1\nint l2;\n#else\nint d2;\n#if Y\nint d3;\n#endif\n#endif\n"
        "#ifdef Z\nint l3;\n#endif\n";
    const auto l = lex(s);
    CHECK(l.is_dead(at(s, "d1")));
    CHECK(l.is_dead(at(s, "d2")));
    CHECK(l.is_dead(at(s, "d3")));
    CHECK(l.is_dead(at(s, "#if Y")));
    CHECK_FALSE(l.is_dead(at(s, "l1")));
    CHECK_FALSE(l.is_dead(at(s, "l2")));
    CHECK_FALSE(l.is_dead(at(s, "l3")));
    CHECK_FALSE(l.is_dead(at(s, "#ifdef Z")));
}

TEST_CASE("includes are directives of live code", "[ops][source_model]") {
    const std::string_view s = "#include \"a.hpp\"\n// #include \"b.hpp\"\n"
        "#if 0\n#include \"c.hpp\"\n#endif\n"
        "const char* x = \"#include <d>\";\n  #  include <e/f.hpp>\n";
    const auto l = lex(s);
    const auto m = model_of(s, l);
    REQUIRE(m.includes.size() == 2);
    CHECK(m.includes[0].target == "a.hpp");
    CHECK_FALSE(m.includes[0].angled);
    CHECK(m.includes[1].target == "e/f.hpp");
    CHECK(m.includes[1].angled);
}

TEST_CASE("a macro definition holds its name and body",
    "[ops][source_model]") {
    const std::string_view s = "#define ORES_X_EXPORT BOOST_SYMBOL_EXPORT\n"
        "#define F(a, b) __declspec(dllimport) \\\n  a\n";
    const auto l = lex(s);
    const auto m = model_of(s, l);
    REQUIRE(m.macros.size() == 2);
    CHECK(m.macros[0].name == "ORES_X_EXPORT");
    CHECK(m.macros[0].body == " BOOST_SYMBOL_EXPORT");
    CHECK(m.macros[1].name == "F");
    CHECK(m.macros[1].body.find("__declspec(dllimport)") !=
        std::string_view::npos);
}

TEST_CASE("declarations are those at namespace scope",
    "[ops][source_model]") {
    const std::string_view s =
        "namespace a::b {\n"
        "struct ORES_X_EXPORT foo final : base<int> { struct nested {}; "
        "void m(); };\n"
        "enum class colour : int { red };\n"
        "template <> struct hash<foo> { };\n"
        "BOOST_MACRO(foo)\n"
        "namespace {\n"
        "static int counter = 0;\n"
        "void helper(int x) { int local; }\n"
        "}\n"
        "constexpr std::array<int, 2> sizes{{1, 2}};\n"
        "void foo::m() {}\n"
        "class fwd;\n"
        "}\n";
    const auto l = lex(s);
    const auto m = model_of(s, l);
    std::vector<std::string> seen;
    for (const auto& d : m.declarations)
        seen.push_back(d.scope + "|" + std::string(d.name) + "|" +
            std::string(d.macro) + "|" + (d.definition ? "def" : "decl") +
            (d.anonymous ? "|anon" : "") + (d.is_static ? "|static" : "") +
            (d.is_constexpr ? "|constexpr" : "") +
            (d.qualified ? "|qualified" : ""));
    CHECK(seen == std::vector<std::string>{
        "a::b|foo|ORES_X_EXPORT|def",
        "a::b|colour||def",
        "a::b|counter||def|anon|static",
        "a::b|helper||def|anon",
        "a::b|sizes||def|constexpr",
        "a::b|m||def|qualified",
        "a::b|fwd||decl"});
    REQUIRE(m.declarations.size() == 7);
    CHECK(m.declarations[0].offset == at(s, "foo final"));
    CHECK(m.declarations[1].kind == declaration_kind::type);
    CHECK(m.declarations[3].kind == declaration_kind::function);
    CHECK(m.declarations[4].kind == declaration_kind::variable);
}

TEST_CASE("an extern \"C\" definition does not hide the declaration after "
    "it", "[ops][source_model]") {
    const std::string_view s = "namespace {\nextern \"C\" void on(int) { "
        "x = 1; }\nstd::string field(int);\n}\n";
    const auto l = lex(s);
    const auto m = model_of(s, l);
    REQUIRE(m.declarations.size() == 2);
    CHECK(m.declarations[0].is_extern);
    CHECK(m.declarations[1].name == "field");
    CHECK(m.declarations[1].anonymous);
}

TEST_CASE("code an #if 0 excludes declares nothing", "[ops][source_model]") {
    const std::string_view s = "namespace a {\n#if 0\nstruct gone {};\n"
        "#endif\nstruct kept {};\n}\n";
    const auto l = lex(s);
    const auto m = model_of(s, l);
    REQUIRE(m.declarations.size() == 1);
    CHECK(m.declarations[0].name == "kept");
}

TEST_CASE("type heads at any scope, with the identifier before the name",
    "[ops][source_model]") {
    const std::string_view s = "enum class ORES_X_EXPORT e : int {};\n"
        "struct s { enum inner { a }; class [[nodiscard]] M n final {}; };\n"
        "using enum e;\nenum f;\nenum g h;\n";
    const auto l = lex(s);
    const auto m = model_of(s, l);
    std::vector<std::string> seen;
    for (const auto& h : m.heads)
        seen.push_back(std::to_string(h.offset) + " " + std::string(h.key) +
            " " + std::string(h.macro) + " " + std::string(h.name));
    CHECK(seen == std::vector<std::string>{"0 enum ORES_X_EXPORT e",
        "37 struct  s", "48 enum  inner", "66 class M n", "117 enum  f",
        "125 enum g h"});
}

TEST_CASE("names are identifiers joined by ::, whether they are called",
    "[ops][source_model]") {
    const std::string_view s = "std :: jthread t; ::ores::f(x); // g()\n";
    const auto l = lex(s);
    const auto m = model_of(s, l);
    REQUIRE(m.names.size() == 4);
    CHECK(components_of_name(m.names[0].text) ==
        std::vector<std::string_view>{"std", "jthread"});
    CHECK_FALSE(m.names[0].called);
    CHECK(m.names[2].text == "::ores::f");
    CHECK(m.names[2].called);
    CHECK(m.names[3].text == "x");
}

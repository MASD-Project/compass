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
#include "compass.cpp/codecs/exported_types.hpp"

#include <optional>
#include <catch2/catch_test_macros.hpp>
#include "compass.cpp/ops/exported_types.hpp"
#include "compass.cpp/ops/lexer.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

constexpr std::string_view export_hpp =
    "#ifndef ORES_A_API_EXPORT_HPP\n"
    "#ifdef ORES_A_API_LIBRARY\n"
    "#define ORES_A_API_EXPORT BOOST_SYMBOL_EXPORT\n"
    "#else\n"
    "#define ORES_A_API_EXPORT BOOST_SYMBOL_IMPORT\n"
    "#endif\n";

/**
 * @brief A library ores.a/api with two exporting headers, one compiled
 * through src/, and a library ores.b/core whose header the compiled one
 * includes.
 */
files library() {
    return {
        {"projects/ores.a/api/include/ores.a.api/export.hpp",
         std::string(export_hpp)},
        {"projects/ores.a/api/include/ores.a.api/compiled.hpp",
         "#include \"ores.b.core/helper.hpp\"\n"
         "class ORES_A_API_EXPORT compiled {};\n"},
        {"projects/ores.a/api/include/ores.a.api/orphan.hpp",
         "#pragma once\n"
         "struct ORES_A_API_EXPORT orphan {};\n"
         "  class ORES_A_API_EXPORT second;\n"},
        {"projects/ores.a/api/src/compiled.cpp",
         "#include \"ores.a.api/compiled.hpp\"\n"},
        {"projects/ores.b/core/include/ores.b.core/helper.hpp",
         "#include \"../ores.b.core/deep.hpp\"\n"},
        {"projects/ores.b/core/include/ores.b.core/deep.hpp", "\n"}};
}

}

TEST_CASE("an exported type no translation unit compiles is reported at "
    "its first declaration", "[codecs][exported_types]") {
    const auto r = tests::run(codecs::exported_types, library());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.a/api/include/ores.a.api/orphan.hpp", 2}});
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<2>(r.findings[0]).starts_with(
        "exports orphan, second with ORES_A_API_EXPORT, "));
}

TEST_CASE("an exported header is compiled when a .cpp reaches it through "
    "another component's include root", "[codecs][exported_types]") {
    auto all = library();
    all["projects/ores.b/core/include/ores.b.core/deep.hpp"] =
        "#include \"ores.a.api/orphan.hpp\"\n";
    CHECK(tests::run(codecs::exported_types, all).findings.empty());
}

TEST_CASE("an include in a comment, a string or an #if 0 block does not "
    "count, and a relative one resolves against the including file",
    "[codecs][exported_types]") {
    auto all = library();
    for (const auto* dead : {
             "// #include \"ores.a.api/orphan.hpp\"\n",
             "/*\n#include \"ores.a.api/orphan.hpp\"\n*/\n",
             "const char* s = \"#include \\\"ores.a.api/orphan.hpp\\\"\";\n",
             "#if 0\n#include \"ores.a.api/orphan.hpp\"\n#endif\n",
             "#if 0\n#if X\n#endif\n#elif 0\n"
             "#include \"ores.a.api/orphan.hpp\"\n#endif\n"}) {
        CAPTURE(dead);
        auto with = all;
        with["projects/ores.a/api/src/compiled.cpp"] += dead;
        CHECK(tests::run(codecs::exported_types, with).findings.size() == 1);
    }
    for (const auto* live : {
             "#if 0\n#else\n#include \"ores.a.api/orphan.hpp\"\n#endif\n",
             "#if 1\n#include \"ores.a.api/orphan.hpp\"\n#endif\n",
             "  #  include \"ores.a.api/orphan.hpp\" // why\n"}) {
        CAPTURE(live);
        auto with = all;
        with["projects/ores.a/api/src/compiled.cpp"] += live;
        CHECK(tests::run(codecs::exported_types, with).findings.empty());
    }

    all["projects/ores.a/api/src/compiled.cpp"] =
        "#include \"ores.a.api/compiled.hpp\"\n"
        "#include \"missing/../../include/ores.a.api/orphan.hpp\"\n";
    CHECK(tests::run(codecs::exported_types, all).findings.size() == 1);
}

TEST_CASE("a library with no src directory, or an export header with no "
    "library guard, is not checked", "[codecs][exported_types]") {
    auto no_sources = library();
    no_sources.erase("projects/ores.a/api/src/compiled.cpp");
    CHECK(tests::run(codecs::exported_types, no_sources).findings.empty());

    auto no_guard = library();
    no_guard["projects/ores.a/api/include/ores.a.api/export.hpp"] =
        "#define ORES_A_API_EXPORT BOOST_SYMBOL_EXPORT\n";
    CHECK(tests::run(codecs::exported_types, no_guard).findings.empty());
}

TEST_CASE("a generated header and another library's macro are not "
    "exports of the library", "[codecs][exported_types]") {
    auto all = library();
    all["projects/ores.a/api/include/ores.a.api/orphan.hpp"] =
        "// AUTO-GENERATED FILE\nstruct ORES_A_API_EXPORT orphan {};\n";
    all["projects/ores.a/api/include/ores.a.api/other.hpp"] =
        "struct ORES_B_CORE_EXPORT other {};\n";
    CHECK(tests::run(codecs::exported_types, all).findings.empty());
}

TEST_CASE("the run is skipped when nothing changed and runs again when a "
    "file it read changes", "[codecs][exported_types]") {
    const auto first = tests::run(codecs::exported_types, library());
    CHECK(tests::run(codecs::exported_types, library(), first.record).ran
        == 0);
    auto edited = library();
    edited["projects/ores.b/core/include/ores.b.core/deep.hpp"] =
        "#include \"ores.a.api/orphan.hpp\"\n";
    const auto again = tests::run(codecs::exported_types, edited,
        first.record);
    CHECK(again.ran == 1);
    CHECK(again.findings.empty());
}

TEST_CASE("an edit that includes an exported header from a source, or "
    "drops the include, changes whether the header is reported",
    "[codecs][exported_types]") {
    auto reached = library();
    reached["projects/ores.a/api/src/compiled.cpp"] +=
        "#include \"ores.a.api/orphan.hpp\"\n";

    CHECK(tests::run_after_edit(codecs::exported_types, library(), reached)
        .findings.empty());
    CHECK(tests::places(tests::run_after_edit(codecs::exported_types,
        reached, library()).findings) == places{
            {"projects/ores.a/api/include/ores.a.api/orphan.hpp", 2}});
}

namespace {

/**
 * @brief A source with its lexed code and model, kept together since the
 * model points into both.
 */
struct read_source {
    explicit read_source(std::string_view s)
        : text(s), lexed(ops::lex(text)), model(ops::model_of(text, lexed)) {}
    std::string text;
    ops::lexed lexed;
    ops::source_model model;
};

std::optional<std::string> export_macro_of(std::string_view s) {
    const read_source r(s);
    const auto m = ops::export_macro(r.lexed, r.model);
    return m ? std::optional<std::string>(*m) : std::nullopt;
}

}

TEST_CASE("the export macro needs its define at a line start and a "
    "library guard", "[ops][exported_types]") {
    CHECK(export_macro_of(export_hpp) == "ORES_A_API_EXPORT");
    CHECK_FALSE(export_macro_of(
        "#ifdef ORES_A_API_LIBRARY\n"
        "x #define ORES_A_API_EXPORT BOOST_SYMBOL_EXPORT\n"));
    CHECK_FALSE(export_macro_of(
        "#ifdef ORES_LIBRARY\n"
        "#define ORES_A_API_EXPORT BOOST_SYMBOL_EXPORT\n"));
    CHECK(export_macro_of(
        "#ifdef ORES_A_LIBRARY_X\n"
        "  #  define ORES_A_EXPORT\tBOOST_SYMBOL_IMPORT\n") ==
        "ORES_A_EXPORT");
    CHECK_FALSE(export_macro_of(
        "#ifdef ORES_A_LIBRARY\n"
        "// #define ORES_A_EXPORT BOOST_SYMBOL_EXPORT\n"));
}

TEST_CASE("quoted includes are read from directives, and angle includes "
    "and includes in comments are not", "[ops][exported_types]") {
    const read_source r(
        "#include \"a.hpp\"\n/* # include  \"b.hpp\" */\n#include <c.hpp>\n"
        "#include \"\"\n#include\"d.hpp\"\n");
    CHECK(ops::quoted_includes(r.model) ==
        std::vector<std::string_view>{"a.hpp", "d.hpp"});
}

TEST_CASE("an exported type is a class or struct the macro decorates",
    "[ops][exported_types]") {
    const read_source r(
        "class ORES_X_EXPORT a {};\r\n"
        "struct\tORES_X_EXPORT b;\n"
        "enum class ORES_X_EXPORT c;\n"
        "class ORES_X_EXPORTS d;\n"
        "class ORES_Y_EXPORT e;\n"
        "/* class ORES_X_EXPORT f; */\n");
    const auto types = ops::exported_types(r.text, r.model);
    REQUIRE(types.size() == 3);
    CHECK(types[0].name == "a");
    CHECK(types[1].name == "b");
    CHECK(types[1].offset == 27);
    CHECK(types[2].macro == "ORES_Y_EXPORT");
}

TEST_CASE("a visibility macro is an object-like define whose body is a "
    "visibility or DLL attribute", "[ops][exported_types]") {
    const read_source r(
        "#define A __declspec ( dllexport )\n"
        " # define B __attribute__ ((visibility(\"default\")))\n"
        "#define C(x) BOOST_SYMBOL_EXPORT\n"
        "#define D \\\nBOOST_SYMBOL_IMPORT\n"
        "#define E __declspec(noinline)\n"
        "x #define F BOOST_SYMBOL_EXPORT\n");
    CHECK(ops::visibility_macros(r.model) ==
        std::vector<std::string_view>{"A", "B", "D"});
}

TEST_CASE("an enum site has an identifier before its name and a "
    "declaration tail, and is not one after using", "[ops][exported_types]") {
    const read_source r(
        "enum class M e : int {};\n"
        "void f(enum E e);\n"
        "using enum M e;\n"
        "enum M { a };\n"
        "xenum M y;\n"
        "int n = 1'000; enum class M g {};\n");
    const auto sites = ops::macro_enum_sites(r.lexed, r.model);
    REQUIRE(sites.size() == 2);
    CHECK(sites[0].offset == 0);
    CHECK(sites[0].macro == "M");
    CHECK(sites[0].source == "enum class M e : int {};");
    CHECK(sites[1].source == "int n = 1'000; enum class M g {};");
}

namespace {

files decorated_tree() {
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
    "enums it decorates", "[codecs][exported_types]") {
    const auto r = tests::run(codecs::exported_types, decorated_tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.a/include/ores.a/kinds.hpp", 2},
        {"projects/ores.a/include/ores.a/kinds.hpp", 5}});
    REQUIRE(r.findings.size() == 2);
    CHECK(std::get<2>(r.findings[0]) == "the visibility macro ORES_A_EXPORT "
        "decorates an enum, and the attribute does not apply to one: "
        "enum class ORES_A_EXPORT colour : int { red };");
}

TEST_CASE("an edit to the file defining a macro changes the findings on "
    "the enums it decorates", "[codecs][exported_types]") {
    const auto export_header = "projects/ores.a/include/ores.a/export.hpp";
    auto after = decorated_tree();
    after[export_header] = "#define ORES_A_EXPORT\n";

    CHECK(tests::run_after_edit(codecs::exported_types, decorated_tree(),
        after).findings.empty());
    CHECK(tests::places(tests::run_after_edit(codecs::exported_types,
        after, decorated_tree()).findings) == places{
            {"projects/ores.a/include/ores.a/kinds.hpp", 2},
            {"projects/ores.a/include/ores.a/kinds.hpp", 5}});
}

TEST_CASE("Boost's own macros are visibility macros without a definition",
    "[codecs][exported_types]") {
    const auto r = tests::run(codecs::exported_types, {
        {"projects/x.cc", "enum struct BOOST_SYMBOL_IMPORT e { a };\n"}});
    CHECK(tests::places(r.findings) == places{{"projects/x.cc", 1}});
}

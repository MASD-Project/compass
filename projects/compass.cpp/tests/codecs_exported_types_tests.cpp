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

#include <catch2/catch_test_macros.hpp>
#include "compass.cpp/ops/exported_types.hpp"
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

TEST_CASE("an include in a comment counts, and a relative one resolves "
    "against the including file", "[codecs][exported_types]") {
    auto all = library();
    all["projects/ores.a/api/src/compiled.cpp"] +=
        "// #include \"../include/ores.a.api/orphan.hpp\"\n";
    CHECK(tests::run(codecs::exported_types, all).findings.empty());

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

TEST_CASE("the export macro needs its define at a line start and a "
    "library guard", "[ops][exported_types]") {
    CHECK(ops::export_macro(export_hpp) == "ORES_A_API_EXPORT");
    CHECK_FALSE(ops::export_macro(
        "#ifdef ORES_A_API_LIBRARY\n"
        "x #define ORES_A_API_EXPORT BOOST_SYMBOL_EXPORT\n"));
    CHECK_FALSE(ops::export_macro(
        "#ifdef ORES_LIBRARY\n"
        "#define ORES_A_API_EXPORT BOOST_SYMBOL_EXPORT\n"));
    CHECK(ops::export_macro(
        "#ifdef ORES_A_LIBRARY_X\n"
        "  #  define ORES_A_EXPORT\tBOOST_SYMBOL_IMPORT\n") ==
        "ORES_A_EXPORT");
}

TEST_CASE("quoted includes are read anywhere and angle includes are not",
    "[ops][exported_types]") {
    CHECK(ops::quoted_includes(
        "#include \"a.hpp\"\n/* # include  \"b.hpp\" */\n#include <c.hpp>\n"
        "#include \"\"\n#include\"d.hpp\"\n") ==
        std::vector<std::string_view>{"a.hpp", "b.hpp"});
}

TEST_CASE("an exported type is a class or struct the macro decorates at "
    "a line start", "[ops][exported_types]") {
    const auto types = ops::exported_types(
        "class ORES_X_EXPORT a {};\r\n"
        "struct\tORES_X_EXPORT b;\n"
        "enum class ORES_X_EXPORT c;\n"
        "class ORES_X_EXPORTS d;\n"
        "class ORES_Y_EXPORT e;\n", "ORES_X_EXPORT");
    REQUIRE(types.size() == 2);
    CHECK(types[0].name == "a");
    CHECK(types[1].name == "b");
    CHECK(types[1].offset == 27);
}

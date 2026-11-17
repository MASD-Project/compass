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
#include "compass.cpp/codecs/unique_type_names.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"projects/ores.a/include/ores.a/one.hpp",
         "namespace ores::a {\n"
         "struct ORES_A_EXPORT widget final : base {\n"
         "    struct inner {};\n"
         "};\n"
         "namespace { class hidden {}; }\n"
         "}\n"},
        {"projects/ores.b/include/ores.b/two.hpp",
         "// struct commented {};\n"
         "#define X struct macro {};\n"
         "namespace ores {\nnamespace a {\n"
         "class widget {};\n"
         "class hidden {};\n"
         "}\n}\n"},
        {"projects/ores.c/tests/fixture.hpp",
         "namespace ores::t { struct helper {}; }\n"},
        {"projects/ores.d/tests/fixture.hpp",
         "namespace ores::t { struct helper {}; }\n"},
        {"projects/ores.e/tests/one.hpp",
         "namespace ores::u { struct twin {}; }\n"},
        {"projects/ores.e/tests/two.hpp",
         "namespace ores::u { struct twin {}; }\n"},
        {"projects/ores.f/src/private.hpp",
         "namespace ores::a { struct widget {}; }\n"}};
}

}

TEST_CASE("a name two headers that can meet define is reported once",
    "[codecs][unique_type_names]") {
    const auto r = tests::run(codecs::unique_type_names, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.a/include/ores.a/one.hpp", 2},
        {"projects/ores.e/tests/one.hpp", 1}});
}

TEST_CASE("an edit that defines a second type of one name is reported "
    "on the first, and one that renames it clears it",
    "[codecs][unique_type_names]") {
    auto before = tree();
    before["projects/ores.b/include/ores.b/two.hpp"] = "namespace x {}\n";
    const auto after = tree();

    CHECK(tests::places(tests::run_after_edit(codecs::unique_type_names,
        before, after).findings) == places{
            {"projects/ores.a/include/ores.a/one.hpp", 2},
            {"projects/ores.e/tests/one.hpp", 1}});
    CHECK(tests::places(tests::run_after_edit(codecs::unique_type_names,
        after, before).findings) == places{
            {"projects/ores.e/tests/one.hpp", 1}});
}

TEST_CASE("an attribute before the name hides no definition",
    "[codecs][unique_type_names]") {
    const files t{
        {"projects/ores.a/include/ores.a/x.hpp",
         "namespace ores::a {\nclass [[nodiscard]] X_EXPORT widget {};\n}\n"},
        {"projects/ores.b/include/ores.b/y.hpp",
         "namespace ores::a {\nstruct widget final {};\n}\n"}};
    CHECK(tests::places(tests::run(codecs::unique_type_names, t).findings) ==
        places{{"projects/ores.a/include/ores.a/x.hpp", 2}});
}

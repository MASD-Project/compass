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
#include "compass.cpp/codecs/translation_unit_budget.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"projects/ores.refdata/api/include/ores.refdata.api/domain/"
         "convention_family_registry.hpp",
         "f(\"ores::refdata::domain::swap\");\n"
         "f(\"ores::refdata::domain::fra\");\n"
         "f(\"ores::refdata::domain::ois\");\n"},
        {"projects/ores.ore/core/src/domain/two.cpp",
         "#include \"conventions_family_pack.hpp\"\n"
         "domain::swap a; domain::fra b; domain::fra c; my_domain::ois d;\n"},
        {"projects/ores.ore/core/src/domain/quiet.cpp",
         "domain::swap a; domain::fra b;\n"},
        {"projects/ores.ore/core/src/domain/one.inc",
         "#include \"conventions_family_pack.hpp\"\ndomain::ois x;\n"},
        {"projects/ores.ore/core/src/domain/"
         "conventions_family_tenor_basis_swap.cpp",
         "#include \"conventions_family_pack.hpp\"\n"
         "domain::swap a; domain::fra b; domain::ois c;\n"},
        {"projects/ores.ore/core/src/domain/"
         "conventions_family_commodity_future.cpp",
         "#include \"conventions_family_pack.hpp\"\n"
         "domain::swap a; domain::fra b; domain::ois c;\n"}};
}

}

TEST_CASE("a unit over its budget and a ceiling above its unit are "
    "reported", "[codecs][translation_unit_budget]") {
    const auto r = tests::run(codecs::translation_unit_budget, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.ore/core/src/domain/two.cpp", 1},
        {"projects/ores.ore/core/src/domain/"
         "conventions_family_commodity_future.cpp", 1},
        {"projects/ores.ore/core/src/domain/"
         "conventions_family_cross_currency_basis.cpp", 0},
        {"projects/ores.ore/core/src/domain/"
         "conventions_family_cross_currency_fix_float.cpp", 0}});
}

TEST_CASE("a family named in a comment or a literal, or a pack included "
    "in a comment, does not count", "[codecs][translation_unit_budget]") {
    auto t = tree();
    t["projects/ores.ore/core/src/domain/two.cpp"] =
        "#include \"conventions_family_pack.hpp\"\n"
        "domain::swap a; // domain::fra b;\nauto s = \"domain::ois\";\n";
    t["projects/ores.ore/core/src/domain/quiet.cpp"] =
        "// #include \"conventions_family_pack.hpp\"\n"
        "domain::swap a; domain::fra b;\n";
    const auto r = tests::run(codecs::translation_unit_budget, t);
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.ore/core/src/domain/"
         "conventions_family_commodity_future.cpp", 1},
        {"projects/ores.ore/core/src/domain/"
         "conventions_family_cross_currency_basis.cpp", 0},
        {"projects/ores.ore/core/src/domain/"
         "conventions_family_cross_currency_fix_float.cpp", 0}});
}

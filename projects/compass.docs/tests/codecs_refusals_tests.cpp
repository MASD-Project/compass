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
#include <array>
#include <functional>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.docs/codecs/recipe_inventories.hpp"
#include "compass.docs/codecs/skill_tables.hpp"
#include "compass.docs/codecs/trade_type_coverage.hpp"
#include "test_tree.hpp"

namespace {

using namespace compass::docs;
using Catch::Matchers::ContainsSubstring;

const std::array generators{
    std::cref(codecs::skill_catalogue),
    std::cref(codecs::skill_type_tables),
    std::cref(codecs::skill_levels),
    std::cref(codecs::recipe_inventories),
    std::cref(codecs::trade_type_coverage)};

}

TEST_CASE("a generator whose inputs are missing says why it writes nothing",
    "[codecs][refusals]") {
    for (const auto& codec : generators) {
        const auto& d = codec.get();
        CAPTURE(d.name);
        const auto findings = tests::run(d, {});
        REQUIRE(findings.size() == 1);
        CHECK_THAT(std::get<2>(findings[0]),
            ContainsSubstring(" is not generated: "));
        REQUIRE(d.rules.size() == 1);
        CHECK(d.rules[0].name == "refused");
    }
}

TEST_CASE("a recipe index with a merge conflict is refused, not rewritten",
    "[codecs][refusals]") {
    const tests::files tree{{"doc/recipes/shell/shell.org",
        "#+title: Shell\n<<<<<<< HEAD\n=======\n>>>>>>> other\n"}};
    CHECK_FALSE(tests::generate(codecs::recipe_inventories, tree).contains(
        "doc/recipes/shell/shell.org"));
    CHECK(tests::messages(tests::run(codecs::recipe_inventories, tree)) ==
        std::vector<std::string>{"doc/recipes/shell/shell.org is not "
            "generated: the index holds merge conflict markers"});
}

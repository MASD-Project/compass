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
#include "compass.crosscheck/codecs/convention_kind_terms.hpp"
#include "compass.crosscheck/codecs/conventions_dataset.hpp"
#include "compass.crosscheck/codecs/curve_sections_wiring.hpp"
#include "compass.crosscheck/codecs/er_diagram.hpp"
#include "compass.crosscheck/codecs/ore_output_catalogue.hpp"
#include "compass.crosscheck/codecs/ore_renderings_tables.hpp"
#include "compass.crosscheck/codecs/route_inventory.hpp"
#include "compass.crosscheck/codecs/script_library_seed.hpp"
#include "compass.crosscheck/codecs/value_sets.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using Catch::Matchers::ContainsSubstring;

const std::array generators{
    std::cref(codecs::curve_sections_wiring),
    std::cref(codecs::curve_column_guards),
    std::cref(codecs::convention_kind_terms),
    std::cref(codecs::ore_renderings_tables),
    std::cref(codecs::script_library_seed),
    std::cref(codecs::ore_output_catalogue),
    std::cref(codecs::er_diagram),
    std::cref(codecs::conventions_dataset),
    std::cref(codecs::route_inventory),
    std::cref(codecs::ore_value_sets)};

}

TEST_CASE("a generator whose inputs are missing writes nothing and says "
    "so", "[codecs][refusals]") {
    for (const auto& codec : generators) {
        const auto& d = codec.get();
        CAPTURE(d.name);
        const auto r = tests::run(d, {});
        CHECK(tests::generate(d, {}).empty());
        REQUIRE(r.findings.size() == 1);
        CHECK_THAT(std::get<2>(r.findings[0]),
            ContainsSubstring("is not generated: "));
        REQUIRE(d.rules.size() == 1);
        CHECK(d.rules[0].name == "refused");
    }
}

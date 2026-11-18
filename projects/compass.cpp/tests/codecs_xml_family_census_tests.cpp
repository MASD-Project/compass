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
#include "compass.cpp/codecs/xml_family_census.hpp"

#include <algorithm>

#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/registry.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;

/**
 * @brief The census with a few families, run through the codec's options.
 */
compass::codec::types::descriptor with_families(std::string_view families) {
    static std::string held;
    static std::array<compass::codec::types::option, 3> options;
    held = families;
    auto d = codecs::xml_family_census;
    std::ranges::copy(d.options, options.begin());
    options[0].default_value = held;
    d.options = options;
    return d;
}

files tree() {
    return {
        {"external/ore/examples/a/ore.xml",
         "<?xml version=\"1.0\"?>\n<!-- <Fake> -->\n<ORE><Setup/></ORE>\n"},
        {"external/ore/examples/b/portfolio.xml", "<Portfolio></Portfolio>\n"},
        {"external/ore/examples/b/sim.xml", "<Simulation/>\n"},
        {"external/ore/examples/b/new.xml", "<Brand.New/>\n"},
        {"projects/ores.ore/core/src/xml/generated/ore.cpp",
         "document_type doc{\"ORE\", rows};\n"},
        {"projects/ores.ore/core/src/xml/generated/sim.cpp",
         "document_type doc{\"Simulation\", rows};\n"},
        {"projects/ores.ore/core/tests/xml_sim_tests.cpp",
         "\n// Simulation round trip\n"},
        {"projects/ores.ore/core/src/xml_sim_tests.cpp", "Simulation\n"}};
}

}

TEST_CASE("each way the census and the tree disagree is reported",
    "[codecs][xml_family_census]") {
    const auto r = tests::run(with_families(
        "ORE=supported,Portfolio=supported,Simulation=unused,"
        "Gone=migrating"), tree());
    std::vector<std::string> messages;
    for (const auto& [path, line, message] : r.findings)
        messages.push_back(path + ":" + std::to_string(line) + " " +
            message.substr(0, message.find(' ', message.find(' ') + 1)));
    CHECK(messages == std::vector<std::string>{
        "external/ore/examples/b/new.xml:1 Brand.New is",
        "external/ore/examples:0 Gone is",
        "projects/ores.ore/core/src/xml/generated:0 Portfolio is",
        "projects/ores.ore/core/src/xml/generated:0 Simulation is",
        "projects/ores.ore/core/tests/xml_sim_tests.cpp:2 names Simulation,"});
}

TEST_CASE("a family that is not name=state is reported",
    "[codecs][xml_family_census]") {
    CHECK(tests::run(with_families("ORE=supported,Portfolio"),
        tree()).findings.size() == 1);
}

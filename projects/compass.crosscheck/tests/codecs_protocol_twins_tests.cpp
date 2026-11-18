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
#include "compass.crosscheck/codecs/protocol_twins.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

}

TEST_CASE("a protocol header of a committed component needs its "
    "TypeScript twin, and no model may opt out",
    "[codecs][protocol_twins]") {
    const files t{
        {"projects/ores.codegen/library/templates/"
         "ores.ts.protocol.protocol_types.org",
         "#+title: ores.ts.protocol.protocol_types\n#+type: archetype\n"
         "#+output: projects/ores.web/generated/{component}/protocol/"
         "{entity}_protocol.ts\n#+facet: ores.ts.protocol\n"},
        {"projects/ores.codegen/library/component_catalogue.org",
         "* Components\n| name | modeling_dir |\n|---+---|\n"
         "| refdata | projects/ores.refdata/modeling |\n"
         "| trading-cpp | projects/ores.trading/modeling |\n"
         "| iam | projects/ores.iam/modeling |\n* Other\n"},
        {"projects/ores.codegen/scripts/component_registry.py",
         "COMPONENTS_UNDER_TEST = (\"refdata\", \"trading\")\n"},
        {"projects/ores.web/generated/refdata/protocol/currency_protocol.ts",
         ""},
        {"projects/ores.web/generated/trading/protocol/trade_protocol.ts", ""},
        {"projects/ores.refdata/api/include/ores.refdata.api/messaging/"
         "currency_protocol.hpp", ""},
        {"projects/ores.refdata/api/include/ores.refdata.api/messaging/"
         "party_protocol.hpp", ""},
        {"projects/ores.trading/api/include/ores.trading.api/messaging/"
         "trade_protocol.hpp", ""},
        {"projects/ores.iam/api/include/ores.iam.api/messaging/"
         "role_protocol.hpp", ""}};
    CHECK(tests::messages_of(tests::run(codecs::protocol_twins, t)) ==
        std::vector<std::string>{
            "expected projects/ores.web/generated/refdata/protocol/"
            "party_protocol.ts (component 'refdata')"});
}

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
#include <catch2/catch_test_macros.hpp>

#include <string>
#include "compass.docs/codecs/trade_type_coverage.hpp"
#include "test_tree.hpp"

namespace {

using namespace compass::docs;

const std::string document = "doc/knowledge/domain/ore_trade_type_coverage.org";
const std::string story = "doc/agile/versions/v0/sprint_30/"
    "hold_every_product_type_the_engine_supports/";

std::string task(std::string_view id, std::string_view state,
    std::string_view title, std::string_view description) {
    return ":PROPERTIES:\n:ID: " + std::string(id) + "\n:END:\n"
        "#+title: Task: " + std::string(title) + "\n#+description: " +
        std::string(description) + "\n\n| State        | " +
        std::string(state) + " |\n";
}

tests::files tree() {
    return {
        {"external/ore/catalogue/trade_types.tsv",
            "trade_type\tin_catalogue\tregistered\tform\telement\t"
            "element_in_schema\tlibrary_scripts\tscript_match\n"
            "Swap\tyes\tyes\tnative\tSwapData\tyes\t-\t-\n"
            "Fancy\tyes\tno\t-\t-\tno\tA B\tpartial\n"
            "OreOnly\tno\tyes\tnative\tX\tyes\t-\t-\n"},
        {"external/ore/catalogue/ore_version.txt",
            "ore v1.8 3d75a6908\nother line\n"},
        {"projects/ores.trading/modeling/"
            "ores.trading.trade_type_catalogue.org",
            "* Trade types\n\n| code | construction | instrument |\n"
            "|---+---+---|\n| Swap | container | swaps |\n"
            "| Fancy | native |  |\n\n* Unsupported trade types\n\n"
            "| code | why |\n| Fancy | no builder |\n"},
        {"projects/ores.synthetic/core/tests/data/generated_trades/Swap.xml",
            "<Trade/>"},
        {story + "task_a.org", task("80315472-7360-4C53-B6F7-57F7BCE78A1C",
            "STARTED", "Tag", "Tags things.")},
        {story + "task_b.org", task("8F1B7D90-0D95-405B-845C-207AA1C92BE3",
            "ABANDONED", "Form", "Forms.")},
        {story + "task_c.org", task("589884B5-1B2C-4044-AAD4-40680C9F3F0C",
            "STARTED", "Both", "Both forms.")},
        {story + "task_d.org", task("D", "BACKLOG", "Swaps",
            "Closes Swap, not Swaption.")},
        {document, "Head.\n# BEGIN GENERATED\nold\n# END GENERATED\nTail.\n"}};
}

}

TEST_CASE("the coverage block counts and tabulates the catalogue's types",
    "[codecs][trade_type_coverage]") {
    const auto out = tests::generate(codecs::trade_type_coverage, tree());

    CHECK(out.at(document) == "Head.\n# BEGIN GENERATED\n\n"
        "Read at ORE commit =3d75a6908=. Regenerate with "
        "=build/scripts/generate_trade_type_coverage.py --write=.\n\n"
        "** Counts\n\n"
        "- The catalogue declares 2 types. ORE registers 1 of them, 0 read "
        "through a =ScriptedTrade= class and 1 natively.\n"
        "- 1 catalogue types have no ORE trade builder: =Fancy=.\n"
        "- ORE registers 1 type the catalogue leaves out: =OreOnly=.\n"
        "- 1 types name an instrument table, and the generator records 1.\n"
        "- 0 registered types state a form in the catalogue that ORE does "
        "not read them as.\n"
        "- 1 types have a library script that carries the same product.\n\n"
        "** Trade types\n\n"
        "| Type | ORE form | Catalogue form | Data element | Table | "
        "Generated | Library scripts | Match | Closed by |\n|-\n"
        "| =Swap= | native | container | =SwapData= | =swaps= | yes | - | - "
        "| [[id:D][Swaps]] |\n"
        "| =Fancy= | not registered | native | - (not in the XSD) | - | no | "
        "=A=, =B= | partial | [[id:80315472-7360-4C53-B6F7-57F7BCE78A1C]"
        "[Tag]], [[id:589884B5-1B2C-4044-AAD4-40680C9F3F0C][Both]] |\n\n"
        "# END GENERATED\nTail.\n");
}

TEST_CASE("an unsupported table that disagrees with ORE stops the codec",
    "[codecs][trade_type_coverage]") {
    auto all = tree();
    auto& catalogue = all.at("projects/ores.trading/modeling/"
        "ores.trading.trade_type_catalogue.org");
    const std::string fancy = "| Fancy | no builder |";
    catalogue.replace(catalogue.find(fancy), fancy.size(),
        "| Swap | no builder |");
    CHECK(tests::generate(codecs::trade_type_coverage, all).empty());
}

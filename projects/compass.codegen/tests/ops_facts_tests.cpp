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
#include "compass.codegen/ops/facts.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.codegen/ops/entity_registry.hpp"

namespace {

using namespace compass;
using codegen::types::entity_registry;
using codegen::types::fact_name;
using codegen::types::fact_reads;

entity_registry registry() {
    entity_registry r;
    r.tables["accounts"] = {"account", codegen::types::parent_entity{}};
    r.tables["books"] = {"book", std::nullopt};
    r.economic_field_groups = {"ores.a.leg"};
    r.convention_families.push_back(codegen::types::convention_family{
        .key = "swaps", .entity = "swap",
        .domain_type = "swap", .subject_stem = "swaps",
        .party_scoped = false, .write_mode = "world", .component = "a",
        .has_wire_codec = false});
    return r;
}

std::map<std::string, store::types::blob_id> facts_of(
    const entity_registry& r) {
    std::vector<codegen::ops::fact> facts;
    codegen::ops::publish_facts(facts, r);
    return {facts.begin(), facts.end()};
}

}

TEST_CASE("a lookup records the element it asked for, found or not, and "
    "nothing else", "[codegen][facts]") {
    auto r = registry();
    fact_reads reads;
    r.record_to(&reads);
    const auto& read = r;

    CHECK(codegen::ops::parent_of(read, "accounts") != nullptr);
    CHECK(read.tables.find("ledgers") == read.tables.end());
    CHECK(read.economic_field_groups.contains("ores.a.leg"));

    CHECK(reads.names() == std::vector<std::string>{
        fact_name("table", "accounts"), fact_name("table", "ledgers"),
        fact_name("economic_field_group", "ores.a.leg")});
}

TEST_CASE("reading every element records the whole collection, once",
    "[codegen][facts]") {
    auto r = registry();
    fact_reads reads;
    r.record_to(&reads);
    const auto& read = r;

    for (const auto& family : read.convention_families)
        CHECK(family.key == "swaps");
    CHECK(std::ranges::find(read.convention_families, "swap",
        &codegen::types::convention_family::entity) !=
        read.convention_families.end());
    const compass::model::ops::custom_types& types = read.custom_types;
    CHECK(types.empty());

    CHECK(reads.names() == std::vector<std::string>{"convention_families",
        "custom_types"});
}

TEST_CASE("a copy shares the elements and records to its own reads",
    "[codegen][facts]") {
    const auto shared = registry();
    auto mine = shared;
    fact_reads reads;
    mine.record_to(&reads);

    CHECK(mine.tables.contains("books"));
    CHECK(shared.tables.contains("accounts"));

    CHECK(reads.names() ==
        std::vector<std::string>{fact_name("table", "books")});
    CHECK(&mine.tables.unrecorded() == &shared.tables.unrecorded());
}

TEST_CASE("a field that changes changes the fact of its element and of "
    "its collection, and no other", "[codegen][facts]") {
    const auto before = facts_of(registry());
    auto r = registry();
    r.tables["accounts"].parent->column_names.insert("opened_on");

    const auto after = facts_of(r);

    REQUIRE(before.size() == after.size());
    std::vector<std::string> changed;
    for (const auto& [name, value] : after) {
        if (before.at(name) != value)
            changed.push_back(name);
    }
    CHECK(changed == std::vector<std::string>{"table",
        fact_name("table", "accounts")});
}

TEST_CASE("a model that stops loading changes the fact of its table",
    "[codegen][facts]") {
    const auto before = facts_of(registry());
    auto r = registry();
    r.tables["accounts"].parent.reset();

    CHECK(facts_of(r).at(fact_name("table", "accounts")) !=
        before.at(fact_name("table", "accounts")));
}

TEST_CASE("every collection has a fact, and a map or a set one per "
    "element", "[codegen][facts]") {
    const auto facts = facts_of(registry());

    for (const auto name : {"table", "trade_type_routes", "custom_types",
             "custom_text_forms",
             "economic_field_group", "directory", "convention_families",
             "shell_units", "shell_local_units"})
        CHECK(facts.contains(name));
    CHECK(facts.contains(fact_name("table", "books")));
    CHECK(facts.contains(fact_name("economic_field_group", "ores.a.leg")));
    CHECK(facts.size() == 12);
}

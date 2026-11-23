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
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "load_fixture.hpp"

namespace {

using compass::model::tests::checkout;
namespace types = compass::model::types;

constexpr std::string_view model_path =
    "projects/compass.db/modeling/compass.db.step_result.org";

const std::string retained_profile = R"org(#+title: Profile: Activity record
#+type: profile

* Assignments

| Feature  | Value |
|----------+-------|
| retained | true  |
)org";

std::string model(std::string_view flags, std::string_view outcome_extra,
    std::string_view rest = {}) {
    return std::string(R"org(#+title: compass.db.step_result
#+type: masd.codegen.entity
#+entity_singular: step_result
#+brief: The outcome of one step of a run.

* Flags
:PROPERTIES:
:product:  compass
:component: db
:schema:   activity
)org") + std::string(flags) + R"org(:END:

* Columns

** run
:PROPERTIES:
:type:         bigint
:primary_key:  true
:key_kind:     owner
:END:

** step_ord
:PROPERTIES:
:type:         integer
:primary_key:  true
:key_kind:     natural
:END:

** outcome
:PROPERTIES:
:type:    text
:values:  pass fail blocked skipped
)org" + std::string(outcome_extra) + R"org(:END:

** heading_ord
:PROPERTIES:
:type:      integer
:nullable:  true
:member:    heading
:END:

* Foreign keys

** run
:PROPERTIES:
:references:  activity.run
:on_delete:   cascade
:END:
)org" + std::string(rest);
}

checkout with(std::string source) {
    checkout c;
    c.add("projects/modeling/variability_activity_record.org",
        retained_profile);
    c.add(std::string(model_path), std::move(source));
    return c;
}

}

TEST_CASE("a masd.codegen.entity model reads into its table and row",
    "[ops][load][masd]") {
    const auto c = with(model(":profile:  activity-record\n",
        ":enum:    step_outcome\n", R"org(
* SQL

** Unique

| columns           |
|-------------------|
| run, heading_ord  |
)org"));
    REQUIRE(c.load_error(model_path).empty());
    const auto e = c.masd_entity(model_path);
    CHECK(e.schema == "activity");
    CHECK(e.row_type == "step_result_row");
    CHECK(e.retained);
    REQUIRE(e.columns.size() == 4);
    CHECK(e.columns[0].key == types::key_kind::owner);
    CHECK(e.columns[2].enum_name == "step_outcome");
    CHECK(e.columns[2].values.size() == 4);
    CHECK(e.columns[3].member == "heading");
    CHECK(e.columns[3].nullable);
    REQUIRE(e.foreign_keys.size() == 1);
    CHECK(e.foreign_keys[0].cascade);
    REQUIRE(e.uniques.size() == 1);
    CHECK(e.uniques[0] == std::vector<std::string>{"run", "heading_ord"});
}

TEST_CASE("a column's value list names its enumeration after the entity "
    "unless the model names it", "[ops][load][masd]") {
    const auto c = with(model("", ""));
    CHECK(c.masd_entity(model_path).columns[2].enum_name ==
        "step_result_outcome");
    CHECK_FALSE(c.masd_entity(model_path).retained);
}

TEST_CASE("a property the format does not have is a diagnostic",
    "[ops][load][masd]") {
    SECTION("on a column") {
        const auto c = with(model("", ":colour:  blue\n"));
        const auto error = c.load_error(model_path);
        CHECK(error.find("the column 'outcome' has the property 'colour'") !=
            std::string::npos);
    }
    SECTION("in * Flags") {
        const auto c = with(model(":tenant:  true\n", ""));
        CHECK(c.load_error(model_path).find(
            "* Flags has the property 'tenant'") != std::string::npos);
    }
    SECTION("as a value outside the property's list") {
        const auto c = with(model("", ":written_by:  someone\n"));
        CHECK(c.load_error(model_path).find("'someone'") !=
            std::string::npos);
    }
}

TEST_CASE("an unknown profile or one with an unknown feature is a "
    "diagnostic", "[ops][load][masd]") {
    SECTION("a profile with no page") {
        const auto c = with(model(":profile:  record-everything\n", ""));
        CHECK(c.load_error(model_path).find(
            "unknown profile 'record-everything'") != std::string::npos);
    }
    SECTION("a profile of ORE Studio's entities") {
        auto c = with(model(":profile:  simple-lookup\n", ""));
        c.add_modeling_documents();
        CHECK(c.load_error(model_path).find(
            "has the property 'has_tenant_id'") != std::string::npos);
    }
}

TEST_CASE("a foreign key or constraint on a column the model lacks is a "
    "diagnostic", "[ops][load][masd]") {
    const auto c = with(model("", "", R"org(
* SQL

** Unique

| columns      |
|--------------|
| run, steps   |
)org"));
    CHECK(c.load_error(model_path).find("names the column 'steps'") !=
        std::string::npos);
}

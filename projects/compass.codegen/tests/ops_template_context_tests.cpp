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
#include "compass.codegen/ops/template_context.hpp"

#include <fstream>
#include <sstream>
#include <catch2/catch_test_macros.hpp>
#include "compass.codegen/ops/entity_steps.hpp"
#include "entity_fixture.hpp"

namespace {

using namespace compass;
using codegen::ops::values::items;
using codegen::ops::values::key_of;
using codegen::ops::values::member;
using model::types::model_tables;
using model::types::value_id;
using model::types::value_kind;

std::string read_data(std::string_view name) {
    std::ifstream in(std::string(COMPASS_CODEGEN_TEST_DATA_DIR) +
        "/entity_surfaces/" + std::string(name));
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

/*
 * The path of the first difference between two value trees, objects
 * compared by member whatever their order; empty when they are equal.
 */
std::string difference(const model_tables& t, value_id a, value_id b,
    const std::string& path) {
    if (a.is_none() || b.is_none())
        return a.is_none() && b.is_none() ? "" : path;
    const auto kind = t.values.kinds[a];
    if (kind != t.values.kinds[b])
        return path;
    const auto ours = items(t, a);
    const auto theirs = items(t, b);
    if (kind == value_kind::object || kind == value_kind::list) {
        if (ours.size() != theirs.size())
            return path + " (size)";
        for (std::size_t i = 0; i < theirs.size(); ++i) {
            const auto d = kind == value_kind::object ?
                difference(t, member(t, a, key_of(t, theirs[i])), theirs[i],
                    path + "." + std::string(key_of(t, theirs[i]))) :
                difference(t, ours[i], theirs[i],
                    path + "[" + std::to_string(i) + "]");
            if (!d.empty())
                return d;
        }
        return "";
    }
    if (kind == value_kind::string)
        return codegen::ops::values::str(t, a) == codegen::ops::values::str(t, b)
            ? "" : path;
    return t.values.scalars[a] == t.values.scalars[b] ? "" : path;
}

/*
 * The surfaces test's entity, enriched as far as the recipes read it, in
 * a context that holds it under domain_entity and under its model key.
 */
struct recipe_fixture {
    codegen::tests::entity_fixture f{read_data("entity.json")};
    value_id context;
    value_id expected;

    recipe_fixture() {
        codegen::ops::derive_filter_record(f.b, f.entity, {});
        codegen::ops::derive_operations(f.b, f.entity);
        codegen::ops::derive_surface_plans(f.b, f.entity);
        context = f.b.detached_object();
        f.b.set_copy(f.b.object(context, "currency_pair"), "domain_entity",
            f.entity);
        f.b.set_copy(context, "domain_entity", f.entity);
        expected = model::io::read_json(f.b, read_data("recipes.json"));
    }

    std::string compare(value_id copy, std::string_view key,
        std::string_view expected_key) {
        return difference(f.tables, member(f.tables, copy, key),
            member(f.tables, expected, expected_key), std::string(key));
    }
};

}

TEST_CASE("recipe ids are uuid5 in the recipe namespace",
    "[codegen][template_context]") {
    CHECK(codegen::ops::recipe_org_id("refdata.currency_pairs-add") ==
        "BEC0D025-8259-50F4-86F9-003F2283AF53");
}

/*
 * Expected documents are shell_recipe_document's and
 * http_recipe_document's for the same plans (data/entity_surfaces).
 */
TEST_CASE("a recipe template reads its document beside the shared context",
    "[codegen][template_context]") {
    recipe_fixture r;
    codegen::types::entity_registry registry;
    const auto shell = codegen::ops::template_context(r.f.b, r.context,
        "currency_pair", "domain_entity", "shell_recipe.org.mustache",
        registry);
    REQUIRE(!shell.is_none());
    CHECK(r.compare(shell, "shell_recipe", "shell_recipe") == "");
    CHECK(member(r.f.tables, r.context, "shell_recipe").is_none());

    const auto http = codegen::ops::template_context(r.f.b, r.context,
        "currency_pair", "domain_entity", "http_recipe.org.mustache",
        registry);
    REQUIRE(!http.is_none());
    CHECK(r.compare(http, "http_recipe", "http_recipe") == "");

    CHECK(codegen::ops::template_context(r.f.b, r.context, "currency_pair",
        "domain_entity", "cpp_service.cpp.mustache", registry).is_none());
}

TEST_CASE("the TypeScript protocol imports the domain types it names",
    "[codegen][template_context]") {
    codegen::tests::entity_fixture f(R"j({"component": "refdata",
        "entity_singular": "currency_pair",
        "messages": [{"fields": [
            {"cpp_type": "ores::utility::domain::order"},
            {"cpp_type": "std::vector<ores::refdata::domain::currency_pair>"},
            {"cpp_type": "ores::marketdata::domain::fx_rate"}]}]})j");
    const auto context = f.b.detached_object();
    f.b.set_copy(f.b.object(context, "pair"), "domain_entity", f.entity);
    f.b.set_copy(context, "domain_entity", f.entity);
    const auto copy = codegen::ops::template_context(f.b, context, "pair",
        "domain_entity", "ts_protocol.ts.mustache", {});
    const auto expected = model::io::read_json(f.b, R"j({
        "utility_imports": [{"name_pascal": "Order",
            "module": "utility/protocol"}],
        "extra_domain_imports": [{"entity": "fx_rate",
            "entity_pascal": "FxRate",
            "module_dir": "../../marketdata/domain"}]})j");
    for (const auto entity : {member(f.tables, copy, "domain_entity"),
             member(f.tables, member(f.tables, copy, "pair"),
                 "domain_entity")}) {
        for (const auto key : {"utility_imports", "extra_domain_imports"}) {
            INFO(key);
            CHECK(difference(f.tables, member(f.tables, entity, key),
                member(f.tables, expected, key), key) == "");
        }
    }
    CHECK(f.json(member(f.tables, context, "domain_entity"),
        "utility_imports") == "<absent>");
}

/*
 * Expected row is _convention_family_row of _entry_for on the same file.
 */
TEST_CASE("a convention family is read from its model's text",
    "[codegen][template_context]") {
    const auto family = codegen::ops::convention_family_of(
        "projects/ores.refdata/modeling/ores.refdata.swap_convention.org",
        read_data("ores.refdata.swap_convention.org"));
    REQUIRE(family);
    codegen::types::entity_registry registry;
    registry.convention_families.push_back(*family);
    recipe_fixture r;
    const auto entity = member(r.f.tables, r.context, "domain_entity");
    r.f.b.set(entity, "entity_singular", "swap_convention");
    const auto copy = codegen::ops::template_context(r.f.b, r.context,
        "currency_pair", "domain_entity",
        "cpp_convention_family_unit.cpp.mustache", registry);
    REQUIRE(!copy.is_none());
    CHECK(r.compare(copy, "convention_unit", "family") == "");

    const auto component = codegen::ops::template_context(r.f.b, r.context,
        "refdata", "component", "ts_convention_family_registry.ts.mustache",
        registry);
    REQUIRE(!component.is_none());
    const auto families = items(r.f.tables, member(r.f.tables,
        member(r.f.tables, component, "convention_registry"), "families"));
    REQUIRE(families.size() == 1);
    CHECK(difference(r.f.tables, families[0],
        member(r.f.tables, r.expected, "family"), "families[0]") == "");

    CHECK_FALSE(codegen::ops::convention_family_of("x/ores.refdata.y.org",
        "#+type: ores.codegen.junction\n#+convention_family: y\n"));
}

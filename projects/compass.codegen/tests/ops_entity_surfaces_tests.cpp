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
#include "compass.codegen/ops/entity_steps.hpp"

#include <fstream>
#include <sstream>
#include <catch2/catch_test_macros.hpp>
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
    if (kind == value_kind::object) {
        const auto ours = items(t, a);
        const auto theirs = items(t, b);
        if (ours.size() != theirs.size())
            return path + " (members)";
        for (const auto m : theirs) {
            const auto key = std::string(key_of(t, m));
            const auto d = difference(t, member(t, a, key), m, path + "." + key);
            if (!d.empty())
                return d;
        }
        return "";
    }
    if (kind == value_kind::list) {
        const auto ours = items(t, a);
        const auto theirs = items(t, b);
        if (ours.size() != theirs.size())
            return path + " (length)";
        for (std::size_t i = 0; i < ours.size(); ++i) {
            const auto d = difference(t, ours[i], theirs[i],
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

void derive(codegen::tests::entity_fixture& f) {
    codegen::types::entity_site site;
    codegen::ops::derive_filter_record(f.b, f.entity, site);
    codegen::ops::derive_operations(f.b, f.entity);
    codegen::ops::derive_surface_plans(f.b, f.entity);
}

}

/*
 * The fixture and the plans are ores.codegen's: tests/data/entity_surfaces
 * holds a currency pair entity with every verb, a scoped read, minted and
 * session-filled columns, and web and classification screens, and the
 * plans entity_shell_plan and its siblings compute for it.
 */
TEST_CASE("the shell, HTTP and web plans match ores.codegen's",
    "[codegen][entity][surfaces]") {
    codegen::tests::entity_fixture f(read_data("entity.json"));
    derive(f);
    const auto expected = model::io::read_json(f.b, read_data("expected.json"));
    for (const auto key : {"shell", "http_route", "web_records",
             "web_classification"}) {
        INFO(key);
        CHECK(difference(f.tables, member(f.tables, f.entity, key),
            member(f.tables, expected, key), key) == "");
    }
}

TEST_CASE("an entity with no screen keys has null web plans",
    "[codegen][entity][surfaces]") {
    codegen::tests::entity_fixture f(R"j({"component": "iam",
        "entity_singular": "tenant", "entity_plural": "tenants",
        "messages": [{"name": "list_tenants_request", "derived": true,
            "subject": "iam.v1.tenants.list", "fields": []}]})j");
    derive(f);
    CHECK(f.json("web_records") == "null");
    CHECK(f.json("web_classification") == "null");
    const auto routes = items(f.tables,
        member(f.tables, member(f.tables, f.entity, "http_route"), "routes"));
    REQUIRE(routes.size() == 1);
    CHECK(f.json(routes[0], "pattern") == R"("/api/v1/iam/tenants")");
    CHECK(f.json(routes[0], "relation") == "{}");
}

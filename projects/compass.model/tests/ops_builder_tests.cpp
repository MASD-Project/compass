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
#include "compass.model/ops/builder.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.model/io/json.hpp"

namespace {

using namespace compass::model;

std::string json(const types::model_tables& t, types::value_id v) {
    std::string out;
    io::write_json(out, t, v);
    return out;
}

}

TEST_CASE("setting a key an object holds replaces its value, as a dict "
    "does", "[model][builder]") {
    types::model_tables t;
    ops::builder b(t);
    const auto root = b.add_model("m.org", types::model_type::entity);
    const auto sql = b.object(root, "sql");
    b.set(sql, "old", true);
    b.object(root, "sql");
    b.set(root, "name", "a");
    b.set_integer(root, "name", 2);
    CHECK(json(t, root) == "{\n  \"name\": 2,\n  \"sql\": {}\n}\n");
}

TEST_CASE("a copy does not change when its source does",
    "[model][builder]") {
    types::model_tables t;
    ops::builder b(t);
    const auto root = b.add_model("m.org", types::model_type::entity);
    const auto source = b.object(root, "source");
    b.set(source, "k", "before");
    b.set_copy(root, "copy", source);
    b.set(source, "k", "after");
    b.remove(root, "source");
    CHECK(json(t, root) == "{\n  \"copy\": {\n    \"k\": \"before\"\n  }\n}\n");
}

TEST_CASE("texts are interned once", "[model][builder]") {
    types::model_tables t;
    ops::builder b(t);
    const auto root = b.add_model("m.org", types::model_type::entity);
    const auto list = b.list(root, "names");
    b.append(list, "names");
    b.append(list, "names");
    CHECK(t.texts.size() == 2);
}

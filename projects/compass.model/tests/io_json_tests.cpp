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
#include "compass.model/io/json.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.model/ops/builder.hpp"

namespace {

using namespace compass::model;

}

/*
 * The expected text is what json.dumps(value, sort_keys=True, indent=2,
 * ensure_ascii=False) writes for the same value.
 */
TEST_CASE("a model is written as Python writes canonical JSON",
    "[model][json]") {
    types::model_tables t;
    ops::builder b(t);
    const auto root = b.add_model("m.org", types::model_type::component);
    b.set(root, "zeta", "\x01\"\\\n\t\xC3\xA9");
    b.set_integer(root, "alpha", -3);
    b.list(root, "empty_list");
    b.object(root, "empty_object");
    const auto list = b.list(root, "mixed");
    b.append(list, true);
    b.append_integer(list, 7);
    b.set_null(b.append_object(list), "n");
    std::string out;
    io::write_json(out, t, root);
    CHECK(out == R"json({
  "alpha": -3,
  "empty_list": [],
  "empty_object": {},
  "mixed": [
    true,
    7,
    {
      "n": null
    }
  ],
  "zeta": "\u0001\"\\\n\t)json" "\xC3\xA9" R"json("
}
)json");
}

TEST_CASE("JSON read back is written as the same text", "[model][json]") {
    const std::string text = R"json({
  "a": [
    null,
    false,
    -12,
    "xé😀\/\"",
    1.5e3,
    [],
    {}
  ],
  "b": {
    "c": true
  }
}
)json";
    types::model_tables t;
    ops::builder b(t);
    const auto root = io::read_json(b, text);
    std::string out;
    io::write_json(out, t, root);
    CHECK(out == R"json({
  "a": [
    null,
    false,
    -12,
    "x)json" "\xC3\xA9\xF0\x9F\x98\x80" R"json(/\"",
    "1.5e3",
    [],
    {}
  ],
  "b": {
    "c": true
  }
}
)json");
}

TEST_CASE("JSON that is not an object or array is refused",
    "[model][json]") {
    types::model_tables t;
    ops::builder b(t);
    CHECK_THROWS(io::read_json(b, "\"text\""));
    CHECK_THROWS(io::read_json(b, "{\"a\": }"));
    CHECK_THROWS(io::read_json(b, "{\"a\": \"open"));
    CHECK_THROWS(io::read_json(b, "[1] 2"));
}

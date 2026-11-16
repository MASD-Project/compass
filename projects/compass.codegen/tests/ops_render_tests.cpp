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
#include "compass.codegen/ops/render.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.codegen/ops/compile.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/builder.hpp"

namespace {

using namespace compass;

/**
 * @brief @p source rendered against the JSON object @p data with no
 * escaping, as ores.codegen's renderer is configured.
 */
std::string render(std::string source, std::string_view data) {
    model::types::model_tables t;
    model::ops::builder b(t);
    const auto context = model::io::read_json(b, data);
    std::string out;
    codegen::ops::render(out, codegen::ops::compile(std::move(source)), t,
        context);
    return out;
}

}

/*
 * Each expected text is what pystache.Renderer(escape=lambda s: s) renders
 * for the same template and data, the cases the Mustache spec leaves out.
 */
TEST_CASE("values render as Python's str() writes them",
    "[codegen][render]") {
    CHECK(render("{{t}} {{f}} {{n}} {{i}} {{z}}",
        R"({"t": true, "f": false, "n": null, "i": 3, "z": 0})") ==
        "True False None 3 0");
    CHECK(render("{{l}}",
        R"({"l": ["a", 1, true, null, {"k": "it's"}]})") ==
        R"(['a', 1, True, None, {'k': "it's"}])");
}

TEST_CASE("nothing is escaped", "[codegen][render]") {
    CHECK(render("{{x}}", R"({"x": "<'&>"})") == "<'&>");
}

TEST_CASE("an empty object and zero are false", "[codegen][render]") {
    CHECK(render("{{#o}}x{{/o}}{{^o}}y{{/o}}|{{#z}}x{{/z}}{{^z}}y{{/z}}",
        R"({"o": {}, "z": 0})") == "y|y");
}

TEST_CASE("a section over a string renders once with it on the stack",
    "[codegen][render]") {
    CHECK(render("a {{#s}}{{.}}{{/s}} b", R"({"s": "str"})") == "a str b");
}

TEST_CASE("a name an item lacks is looked up below it",
    "[codegen][render]") {
    CHECK(render("{{#l}}{{n}}{{/l}}",
        R"({"n": "outer", "l": [{"n": 1}, {}, "x"]})") == "1outerouter");
}

TEST_CASE("a dotted name does not fall back once its first part is found",
    "[codegen][render]") {
    CHECK(render("{{#c}}[{{a.b}}]{{/c}}",
        R"({"a": {"b": 1}, "c": {"a": {}}})") == "[]");
}

TEST_CASE("whitespace after a tag keeps it from standing alone",
    "[codegen][render]") {
    CHECK(render("{{#a}} \nx{{/a}}", R"({"a": true})") == " \nx");
    CHECK(render("  {{! one\n two }}  \nz", "{}") == "    \nz");
}

TEST_CASE("a standalone tag takes its indent and its CRLF line end",
    "[codegen][render]") {
    CHECK(render("\t{{#a}}\r\n.\n  {{/a}}\n", R"({"a": 1})") == ".\n");
}

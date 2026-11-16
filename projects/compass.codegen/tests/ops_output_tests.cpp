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
#include "compass.codegen/ops/output.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.codegen/ops/compile.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/builder.hpp"

namespace {

using namespace compass;

std::string render(std::string_view name, std::string source,
    std::string_view data) {
    model::types::model_tables t;
    model::ops::builder b(t);
    const auto context = model::io::read_json(b, data);
    std::string out;
    codegen::ops::render_output(out, name,
        codegen::ops::compile(std::move(source)), t, context);
    return out;
}

}

/*
 * The expected texts are what ores.codegen's render_template and
 * _substitute_paste_markers return for the same template and data.
 */
TEST_CASE("a licence gets the marker of the language the template emits",
    "[codegen][output]") {
    CHECK(render("cmake_x.mustache", "{{{cmake_license}}}|{{{cpp_license}}}",
        R"({"cmake_license": "# L", "cpp_license": "/* L */"})") ==
        "# L\n# AUTO-GENERATED FILE - DO NOT EDIT MANUALLY\n"
        "# Template: cmake_x.mustache\n"
        "# To modify, update the template and regenerate.|/* L */");
    CHECK(render("x.hpp.mustache", "{{{cpp_license}}}",
        R"({"cpp_license": "/* L */"})") ==
        "/* L */\n/**\n * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY\n"
        " * Template: x.hpp.mustache\n"
        " * To modify, update the template and regenerate.\n */");
    CHECK(render("x.sql.mustache", "{{{cpp_license}}}",
        R"({"cpp_license": "/* L */"})") == "/* L */");
}

TEST_CASE("a member template initialises scalar fields only",
    "[codegen][output]") {
    const std::string source =
        "{{#fields}}{{cpp_type}}={{cpp_value_init}};{{/fields}}";
    const std::string data = R"({"fields": [{"cpp_type": "int"},
        {"cpp_type": "std::string", "cpp_value_init": "x"}, {}],
        "cpp_value_init": "outer"})";
    CHECK(render("cpp_protocol.hpp.mustache", source, data) ==
        "int={};std::string=;=outer;");
    CHECK(render("cpp_other.hpp.mustache", source, data) ==
        "int=outer;std::string=x;=outer;");
}

TEST_CASE("an org output gets its marker after the first drawer end",
    "[codegen][output]") {
    CHECK(render("x.org.mustache", "a\n:END:\r\nb\n", "{}") ==
        "a\n:END:\n#+generated: AUTO-GENERATED FILE - DO NOT EDIT MANUALLY. "
        "Template: x.org.mustache. To modify, update the template and "
        "regenerate.\nb\n");
    CHECK(render("x.org.mustache", "no drawer", "{}") ==
        "#+generated: AUTO-GENERATED FILE - DO NOT EDIT MANUALLY. "
        "Template: x.org.mustache. To modify, update the template and "
        "regenerate.\nno drawer");
}

TEST_CASE("paste markers take the model's implementations of their kind",
    "[codegen][output]") {
    CHECK(render("x.mustache",
        "a\n  <<paste:AB-12>>  \nb <<paste:cd>> c\n<<paste:zz>>\n",
        R"({"domain_entity": {"implementations":
            {"ab-12": ["one  \n", "two"], "CD": []}}})") ==
        "a\none\n\ntwo\nbc\n<<paste:zz>>\n");
}

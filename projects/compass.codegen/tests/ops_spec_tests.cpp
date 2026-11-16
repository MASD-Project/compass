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
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "compass.codegen/ops/compile.hpp"
#include "compass.codegen/ops/render.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/builder.hpp"

namespace {

using namespace compass;
using model::types::model_tables;
using model::types::value_id;

std::string read_file(const std::filesystem::path& p) {
    std::ifstream is(p, std::ios::binary);
    std::ostringstream ss;
    ss << is.rdbuf();
    return ss.str();
}

value_id child(const model_tables& t, value_id object, std::string_view key) {
    for (auto c = t.values.firsts[object]; !c.is_none();
         c = t.values.nexts[c]) {
        if (model::types::text_of(t.texts, t.values.keys[c]) == key)
            return c;
    }
    return value_id::none();
}

std::string_view string_of(const model_tables& t, value_id v) {
    return model::types::text_of(t.texts, model::types::text_id{
        static_cast<std::uint32_t>(t.values.scalars[v])});
}

}

/*
 * Every required test of the Mustache spec v1.4.3, rendered with HTML
 * escaping as pystache's default Renderer renders it. Where pystache
 * disagrees with the spec, pystache_deviations.json, which
 * build/scripts/mustache_spec_deviations.py writes, holds what pystache
 * renders, and that is expected instead.
 */
TEST_CASE("the Mustache spec renders as pystache renders it",
    "[codegen][spec]") {
    const std::filesystem::path dir = COMPASS_CODEGEN_SPEC_DIR;
    const auto file = GENERATE(as<std::string>{}, "comments", "delimiters",
        "interpolation", "inverted", "partials", "sections");

    model_tables t;
    model::ops::builder b(t);
    const auto spec = model::io::read_json(b,
        read_file(dir / (file + ".json")));
    const auto deviations = model::io::read_json(b,
        read_file(dir / "pystache_deviations.json"));

    const auto tests = child(t, spec, "tests");
    std::size_t count = 0;
    for (auto test = t.values.firsts[tests]; !test.is_none();
         test = t.values.nexts[test]) {
        const std::string name(string_of(t, child(t, test, "name")));
        const auto deviation = child(t, deviations, file + "/" + name);
        const std::string expected(string_of(t, deviation.is_none()
            ? child(t, test, "expected") : deviation));
        const auto partials = child(t, test, "partials");

        codegen::ops::render_options options;
        options.escape = codegen::ops::escaping::html;
        options.partials = [&](std::string_view key)
            -> std::optional<std::string_view> {
            if (partials.is_none())
                return std::nullopt;
            const auto p = child(t, partials, key);
            if (p.is_none())
                return std::nullopt;
            return string_of(t, p);
        };

        const auto compiled = codegen::ops::compile(
            std::string(string_of(t, child(t, test, "template"))));
        std::string out;
        codegen::ops::render(out, compiled, t, child(t, test, "data"),
            options);
        CAPTURE(file, name);
        CHECK(out == expected);
        ++count;
    }
    CHECK(count > 0);
}

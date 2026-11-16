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
#include "compass.codegen/ops/context.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.codegen/ops/compile.hpp"
#include "compass.codegen/ops/render.hpp"
#include "compass.model/io/json.hpp"

namespace {

using namespace compass;

constexpr std::string_view modeline_org = R"(* ores.modelines :module:
*** ores.modelines.emacs.sql :modeline:
**** content :attribute:
:PROPERTIES:
:masd.codec.value: mode: s
:END:
*** ores.modelines.emacs.c++ :modeline:
:masd.codec.value: mode: c++
*** ores.modelines.emacs.cmake :modeline:
:masd.codec.value: mode: x
*** ores.modelines.emacs.typescript :modeline:
:masd.codec.value: mode: y
)";

std::string render(const model::types::model_tables& t,
    model::types::value_id context, std::string source) {
    std::string out;
    codegen::ops::render(out, codegen::ops::compile(std::move(source)), t,
        context);
    return out;
}

}

/*
 * The licence blocks are what core.generate_license_with_header returns
 * for the same text, modeline and year.
 */
TEST_CASE("the library values hold a licence block per language",
    "[codegen][context]") {
    model::types::model_tables t;
    model::ops::builder b(t);
    const auto library = codegen::ops::library_values(b,
        "Line one.\n\nLine two.", modeline_org, 2026);
    CHECK(render(t, library, "{{{cmake_license}}}") ==
        "# -*- mode: x -*-\n#\n"
        "# Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>\n"
        "#\n# Line one.\n#\n# Line two.");
    CHECK(render(t, library, "{{{cpp_license}}}") ==
        "/* -*- mode: c++ -*-\n *\n"
        " * Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>\n"
        " *\n * Line one.\n *\n * Line two.\n */");
    CHECK(render(t, library, "{{ts_license}}").starts_with(
        "/** -*- mode: y -*-\n"));
    CHECK(render(t, library, "{{modelines.sql}}|{{sql_modeline}}") ==
        "mode: s|mode: s");
}

TEST_CASE("a component context derives the names generation uses",
    "[codegen][context]") {
    model::types::model_tables t;
    model::ops::builder b(t);
    const auto library = codegen::ops::library_values(b, "L", modeline_org,
        2026);
    const auto part = model::io::read_json(b, R"({"component": {
        "name": "refdata.api", "full_name": "ores.refdata.api",
        "implementations": {"2CD994C9-83FF-4C41-8909-5AD2BF3DECCC": ["x"]}}})");
    const auto context = codegen::ops::model_context(b, library, part,
        "component_overview", "component");
    CHECK(render(t, context,
        "{{#component}}{{full_name_upper}} {{name_upper}} {{namespace}} "
        "{{base_name}} {{segment}} {{api_full_name}} {{core_full_name}} "
        "{{service_full_name}} {{has_app_run_body}} "
        "{{has_app_config_decl}}{{/component}}") ==
        "ORES_REFDATA_API REFDATA_API ores::refdata::api refdata api "
        "ores.refdata.api ores.refdata.core ores.refdata.service True False");
    CHECK(render(t, context, "{{component_overview.component.namespace}}") ==
        "ores::refdata::api");

    const auto flat = model::io::read_json(b, R"({"component":
        {"name": "iam"}})");
    const auto flat_context = codegen::ops::model_context(b, library, flat,
        "component_overview", "component");
    CHECK(render(t, flat_context,
        "{{#component}}{{namespace}}|{{segment}}|{{api_full_name}}"
        "{{/component}}") == "ores::iam||ores.iam.api");
}

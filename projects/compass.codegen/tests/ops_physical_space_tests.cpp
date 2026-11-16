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
#include "compass.codegen/ops/physical_space.hpp"

#include <catch2/catch_test_macros.hpp>
#include "compass.codegen/ops/manifest.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/builder.hpp"

namespace {

using namespace compass;

const codegen::ops::node_document nodes[] = {
    {"ores.cmake.component.org",
     "#+title: ores.cmake.component\n#+type: facet\n"
     "#+facet_group: ores.cmake\n#+model_types: component\n"},
    {"ores.cmake.component.root.org",
     "#+title: ores.cmake.component.root\n#+type: archetype\n"
     "#+facet: ores.cmake.component\n#+component_kind: flat\n"
     "#+output: projects/{component_dir}/CMakeLists.txt\n"
     "* Template\n#+begin_src mustache :tangle cmake_component_root.mustache\n"},
    {"ores.cmake.component.extra.org",
     "#+title: ores.cmake.component.extra\n#+type: archetype\n"
     "#+facet: ores.cmake.component\n#+default: off\n"
     "#+output: projects/{component_dir}/include/{component_include_root}/x\n"
     "#+begin_src mustache :tangle extra.mustache\n"},
    {"ores.cpp.entity.org",
     "#+title: ores.cpp.entity\n#+type: facet\n"
     "#+model_types: domain_entity\n"},
    {"ores.cpp.entity.class.org",
     "#+title: ores.cpp.entity.class\n#+type: archetype\n"
     "#+facet: ores.cpp.entity\n#+output: x.hpp\n"
     "#+begin_src mustache :tangle class.mustache\n"}};

}

TEST_CASE("the graph reads facets and archetypes from node headers",
    "[codegen][physical_space]") {
    const auto g = codegen::ops::load_physical_space(nodes);
    REQUIRE(g.facets.size() == 2);
    REQUIRE(g.archetypes.size() == 3);
    CHECK(g.archetypes.template_names[0] == "cmake_component_root.mustache");
    CHECK(g.archetypes.model_types[0] ==
        std::vector<std::string>{"component"});
    CHECK(g.archetypes.kinds[0] == std::vector<std::string>{"flat"});
    CHECK(g.archetypes.defaults[0] == 1);
    CHECK(g.archetypes.defaults[1] == 0);
}

TEST_CASE("a component renders the archetypes of its kind it enables",
    "[codegen][physical_space]") {
    const auto g = codegen::ops::load_physical_space(nodes);
    model::types::model_tables t;
    model::ops::builder b(t);
    const auto flat = model::io::read_json(b, R"({"component":
        {"name": "http.assets", "full_name": "ores.http.assets",
         "kind": "flat"}})");
    const std::string path = "projects/ores.http/assets/modeling/c.org";

    auto units = codegen::ops::model_units(g, "component", t, flat, path, {});
    REQUIRE(units.size() == 1);
    CHECK(units[0].output == "projects/ores.http/assets/CMakeLists.txt");

    const codegen::ops::drawer_property on[] = {
        {"ores.cmake.component.extra.enabled", "yes"}};
    units = codegen::ops::model_units(g, "component", t, flat, path, on);
    REQUIRE(units.size() == 2);
    CHECK(units[1].output ==
        "projects/ores.http/assets/include/ores.http.assets/x");

    const auto adapter = model::io::read_json(b, R"({"component":
        {"name": "http.assets", "full_name": "ores.http.assets",
         "kind": "adapter"}})");
    units = codegen::ops::model_units(g, "component", t, adapter, path, on);
    REQUIRE(units.size() == 1);
    CHECK(units[0].output == "projects/ores.http/assets/include/ores.http/x");

    const codegen::ops::drawer_property off[] = {
        {"ores.cmake.enabled", "false"}};
    CHECK(codegen::ops::model_units(g, "component", t, flat, path, off)
        .empty());
}

TEST_CASE("regenerate --all reaches the catalogue's modeling directories",
    "[codegen][manifest]") {
    const auto components = codegen::ops::read_component_catalogue(
        "* Components\n\n| name | modeling_dir | exclude_org_types |\n"
        "|------+--------------+-------------------|\n"
        "| a | projects/ores.a/modeling | junction |\n"
        "| b | | |\n* Next\n");
    REQUIRE(components.size() == 2);
    CHECK(components[0].excluded_types ==
        std::vector<std::string>{"junction"});
    const std::vector<std::string> paths{
        "projects/ores.a/modeling/e.org",
        "projects/ores.a/modeling/j.org",
        "projects/ores.a/modeling/notes.org",
        "projects/ores.a/api/modeling/component_overview.org",
        "projects/ores.b/modeling/component_overview.org"};
    const auto models = codegen::ops::regenerated_models(components, paths,
        [](std::string_view p) -> std::string {
            if (p.ends_with("e.org"))
                return "ores.codegen.entity";
            if (p.ends_with("j.org"))
                return "ores.codegen.junction";
            return "";
        });
    CHECK(models == std::set<std::string, std::less<>>{
        "projects/ores.a/api/modeling/component_overview.org",
        "projects/ores.a/modeling/e.org"});
}

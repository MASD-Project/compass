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

#include <algorithm>
#include <map>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <vector>
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

    auto units = codegen::ops::model_units(g, {}, "component", t, flat, path, {});
    REQUIRE(units.size() == 1);
    CHECK(units[0].output == "projects/ores.http/assets/CMakeLists.txt");

    const codegen::ops::drawer_property on[] = {
        {"ores.cmake.component.extra.enabled", "yes"}};
    units = codegen::ops::model_units(g, {}, "component", t, flat, path, on);
    REQUIRE(units.size() == 2);
    CHECK(units[1].output ==
        "projects/ores.http/assets/include/ores.http.assets/x");

    const auto adapter = model::io::read_json(b, R"({"component":
        {"name": "http.assets", "full_name": "ores.http.assets",
         "kind": "adapter"}})");
    units = codegen::ops::model_units(g, {}, "component", t, adapter, path, on);
    REQUIRE(units.size() == 1);
    CHECK(units[0].output == "projects/ores.http/assets/include/ores.http/x");

    const codegen::ops::drawer_property off[] = {
        {"ores.cmake.enabled", "false"}};
    CHECK(codegen::ops::model_units(g, {}, "component", t, flat, path, off)
        .empty());
}

namespace {

/*
 * Entity facets and archetypes as ORE Studio's library declares them, with
 * their output patterns verbatim.
 */
const codegen::ops::node_document entity_nodes[] = {
    {"ores.cpp.domain.class_header.org",
     "#+title: ores.cpp.domain.class_header\n#+type: archetype\n"
     "#+facet: ores.cpp.domain\n#+output: projects/{component_dir}/include/"
     "ores.{component_include}/domain/{entity}.hpp\n"
     ":tangle cpp_domain_type_class.hpp.mustache\n"},
    {"ores.cpp.domain.org",
     "#+title: ores.cpp.domain\n#+type: facet\n"
     "#+model_types: schema domain_entity junction\n"
     "#+facet_group: ores.cpp\n"},
    {"ores.cpp.history-provider-registrar.header.org",
     "#+title: ores.cpp.history-provider-registrar.header\n"
     "#+type: archetype\n#+facet: ores.cpp.history-provider-registrar\n"
     "#+output: projects/{component_core_dir}/include/ores.{component_core}/"
     "messaging/{entity}_history_provider_registrar.hpp\n"
     ":tangle cpp_history_provider_registrar.hpp.mustache\n"},
    {"ores.cpp.history-provider-registrar.org",
     "#+title: ores.cpp.history-provider-registrar\n#+type: facet\n"
     "#+model_types: domain_entity junction\n#+facet_group: ores.cpp\n"},
    {"ores.cpp.http-route.org",
     "#+title: ores.cpp.http-route\n#+type: facet\n"
     "#+model_types: domain_entity junction operation\n"
     "#+facet_group: ores.cpp\n#+default: disabled\n"},
    {"ores.cpp.http-route.web_records_header.org",
     "#+title: ores.cpp.http-route.web_records_header\n#+type: archetype\n"
     "#+facet: ores.cpp.http-route\n#+model_types: domain_entity junction\n"
     "#+output: projects/ores.http/{component}/include/ores.http/routes/"
     "{component}/{web_entity}_routes.hpp\n"
     ":tangle cpp_http_route_web_records.hpp.mustache\n"},
    {"ores.cpp.presentation.history_field_mapper_header.org",
     "#+title: ores.cpp.presentation.history_field_mapper_header\n"
     "#+type: archetype\n#+facet: ores.cpp.presentation\n"
     "#+output: projects/{component_core_dir}/include/ores.{component_core}/"
     "presentation/{entity}_history_field_mapper.hpp\n"
     ":tangle cpp_history_field_mapper.hpp.mustache\n"},
    {"ores.cpp.presentation.org",
     "#+title: ores.cpp.presentation\n#+type: facet\n"
     "#+model_types: schema domain_entity junction\n"
     "#+facet_group: ores.cpp\n"},
    {"ores.cpp.protocol.org",
     "#+title: ores.cpp.protocol\n#+type: facet\n"
     "#+model_types: domain_entity schema junction operation family\n"
     "#+facet_group: ores.cpp\n"},
    {"ores.cpp.protocol.protocol_header.org",
     "#+title: ores.cpp.protocol.protocol_header\n#+type: archetype\n"
     "#+facet: ores.cpp.protocol\n#+output: projects/{component_dir}/include/"
     "ores.{component_include}/messaging/{entity}_protocol.hpp\n"
     ":tangle cpp_protocol.hpp.mustache\n"},
    {"ores.cpp.shell-command.command_aggregator_header.org",
     "#+title: ores.cpp.shell-command.command_aggregator_header\n"
     "#+type: archetype\n#+facet: ores.cpp.shell-command\n"
     "#+model_types: domain_entity junction\n"
     "#+output: projects/ores.shell/application/include/ores.shell/app/"
     "commands/{component}/{component}_commands.hpp\n"
     ":tangle cpp_shell_command_aggregator_header.hpp.mustache\n"},
    {"ores.cpp.shell-command.command_header.org",
     "#+title: ores.cpp.shell-command.command_header\n#+type: archetype\n"
     "#+facet: ores.cpp.shell-command\n#+model_types: domain_entity junction\n"
     "#+output: projects/ores.shell/{component}/include/ores.shell/app/"
     "commands/{component}/{entity}_commands.hpp\n"
     ":tangle cpp_shell_command_header.hpp.mustache\n"},
    {"ores.cpp.shell-command.org",
     "#+title: ores.cpp.shell-command\n#+type: facet\n"
     "#+model_types: domain_entity junction operation\n"
     "#+facet_group: ores.cpp\n#+default: disabled\n"},
    {"ores.doc.shell-recipe.org",
     "#+title: ores.doc.shell-recipe\n#+type: facet\n"
     "#+model_types: domain_entity junction operation\n"
     "#+facet_group: ores.doc\n#+default: enabled\n"},
    {"ores.doc.shell-recipe.recipe.org",
     "#+title: ores.doc.shell-recipe.recipe\n#+type: archetype\n"
     "#+facet: ores.doc.shell-recipe\n"
     "#+model_types: domain_entity junction operation\n"
     "#+requires_facet: ores.cpp.shell-command\n"
     "#+output: doc/recipes/shell/{shell_menu}/{entity}.org\n"
     ":tangle shell_recipe.org.mustache\n"},
    {"ores.sql.schema.domain_entity_create.org",
     "#+title: ores.sql.schema.domain_entity_create\n#+type: archetype\n"
     "#+facet: ores.sql.schema\n#+output: projects/ores.sql/create/"
     "{component}/{component}_{entity_plural}_create.sql\n"
     ":tangle sql_schema_domain_entity_create.mustache\n"},
    {"ores.sql.schema.org",
     "#+title: ores.sql.schema\n#+type: facet\n"
     "#+model_types: domain_entity junction schema\n"
     "#+facet_group: ores.sql\n"}};

const std::string_view entity_path =
    "projects/ores.analytics/modeling/"
    "ores.analytics.credit_simulation_config.org";

const codegen::ops::drawer_property shell_on[] = {
    {"ores.cpp.shell-command.enabled", "true"}};

std::vector<std::string> outputs(const std::vector<codegen::ops::unit>& us) {
    std::vector<std::string> out;
    for (const auto& u : us)
        out.push_back(u.output);
    return out;
}

/**
 * @brief The outputs model_units lists for the domain entity @p json
 * (the members of its domain_entity) at entity_path.
 */
std::vector<std::string> entity_outputs(const std::string& json,
    const codegen::types::checkout_registry& registry = {},
    std::span<const codegen::ops::drawer_property> properties = shell_on) {
    const auto g = codegen::ops::load_physical_space(entity_nodes);
    model::types::model_tables t;
    model::ops::builder b(t);
    const auto model = model::io::read_json(b,
        "{\"domain_entity\": " + json + "}");
    return outputs(codegen::ops::model_units(g, registry, "domain_entity", t,
        model, entity_path, properties));
}

const std::string credit_simulation_config = R"({"component": "analytics",
    "subcomponent": "api", "entity_singular": "credit_simulation_config",
    "entity_plural": "credit_simulation_configs"})";

}

/*
 * Expected paths are the ones ores.codegen wrote for ORE Studio's
 * ores.analytics.credit_simulation_config and ores.refdata.business_centre.
 */
TEST_CASE("a domain entity's outputs resolve its component and names",
    "[codegen][physical_space]") {
    CHECK(entity_outputs(credit_simulation_config) ==
        std::vector<std::string>{
            "projects/ores.analytics/api/include/ores.analytics.api/domain/"
            "credit_simulation_config.hpp",
            "projects/ores.analytics/core/include/ores.analytics.core/"
            "messaging/credit_simulation_config_history_provider_registrar.hpp",
            "projects/ores.analytics/core/include/ores.analytics.core/"
            "presentation/credit_simulation_config_history_field_mapper.hpp",
            "projects/ores.analytics/api/include/ores.analytics.api/"
            "messaging/credit_simulation_config_protocol.hpp",
            "projects/ores.shell/analytics/include/ores.shell/app/commands/"
            "analytics/credit_simulation_config_commands.hpp",
            "doc/recipes/shell/credit_simulation_configs/"
            "credit_simulation_config.org",
            "projects/ores.sql/create/analytics/"
            "analytics_credit_simulation_configs_create.sql"});

    const auto off = entity_outputs(credit_simulation_config, {}, {});
    CHECK(std::ranges::none_of(off, [](const std::string& o) {
        return o.contains("ores.shell") || o.contains("recipes/shell");
    }));
}

TEST_CASE("a web placeholder resolves only for an entity that states it",
    "[codegen][physical_space]") {
    const codegen::ops::drawer_property http_on[] = {
        {"ores.cpp.http-route.enabled", "true"}};
    const std::string web = R"({"component": "refdata",
        "subcomponent": "api", "entity_singular": "business_centre",
        "web_resource": "business-centres"})";
    const std::string plain = R"({"component": "refdata",
        "subcomponent": "api", "entity_singular": "business_centre"})";
    const std::string route = "projects/ores.http/refdata/include/ores.http/"
        "routes/refdata/business_centre_web_routes.hpp";

    CHECK(std::ranges::count(entity_outputs(web, {}, http_on), route) == 1);
    CHECK(std::ranges::count(entity_outputs(plain, {}, http_on), route) == 0);
}

TEST_CASE("an operation model that owns the protocol takes it from the entity",
    "[codegen][physical_space]") {
    codegen::types::checkout_registry registry;
    registry.protocol_owners.emplace("projects/ores.analytics/modeling",
        "analytics", "credit_simulation_config");
    const auto units = entity_outputs(credit_simulation_config, registry);
    CHECK(std::ranges::none_of(units, [](const std::string& o) {
        return o.contains("_protocol.hpp") || o.contains("ores.shell") ||
            o.contains("recipes/shell");
    }));
    CHECK(units.size() == 4);

    registry.protocol_owners = {{"projects/ores.other/modeling", "analytics",
        "credit_simulation_config"}};
    CHECK(entity_outputs(credit_simulation_config, registry).size() == 7);
}

TEST_CASE("a current-state entity renders no history surface",
    "[codegen][physical_space]") {
    const auto units = entity_outputs(R"({"component": "analytics",
        "subcomponent": "api", "entity_singular": "credit_simulation_config",
        "entity_plural": "credit_simulation_configs",
        "sql": {"current_state": true}})");
    CHECK(std::ranges::none_of(units, [](const std::string& o) {
        return o.contains("history");
    }));
    CHECK(units.size() == 5);

    const auto audit = entity_outputs(R"({"component": "analytics",
        "subcomponent": "api", "entity_singular": "credit_simulation_config",
        "domain_audit_group": "audit"})");
    CHECK(std::ranges::count_if(audit, [](const std::string& o) {
        return o.contains("history");
    }) == 1);
}

TEST_CASE("a staging shim renders no menu another component also models",
    "[codegen][physical_space]") {
    const std::string shim = R"({"component": "analytics",
        "subcomponent": "api", "entity_singular": "credit_simulation_config",
        "entity_plural": "credit_simulation_configs",
        "profile": "simple-lookup, artefact-staging-only"})";
    const auto has_menu = [](const std::vector<std::string>& units) {
        return std::ranges::any_of(units, [](const std::string& o) {
            return o.contains("ores.shell");
        });
    };
    codegen::types::checkout_registry registry;
    registry.plural_owners["credit_simulation_configs"] = {"ores.analytics"};
    CHECK(has_menu(entity_outputs(shim, registry)));

    registry.plural_owners["credit_simulation_configs"].emplace("ores.dq");
    CHECK(!has_menu(entity_outputs(shim, registry)));
}

TEST_CASE("the shell registrar renders where the component opts into it",
    "[codegen][physical_space]") {
    const std::string swap = R"({"component": "trading",
        "subcomponent": "api", "entity_singular": "vanilla_swap_instrument"})";
    const std::string aggregator = "projects/ores.shell/application/include/"
        "ores.shell/app/commands/trading/trading_commands.hpp";
    CHECK(std::ranges::count(entity_outputs(swap), aggregator) == 0);

    codegen::types::checkout_registry registry;
    registry.shell_registrars.emplace("trading");
    CHECK(std::ranges::count(entity_outputs(swap, registry), aggregator) == 1);
}

TEST_CASE("a store-only entity renders only its SQL",
    "[codegen][physical_space]") {
    CHECK(entity_outputs(R"({"component": "compute",
        "entity_singular": "grid_sample", "entity_plural": "grid_samples",
        "sql_only": true})") == std::vector<std::string>{
            "projects/ores.sql/create/compute/compute_grid_samples_create.sql"});
}

namespace {

/**
 * @brief The outputs model_units lists for the junction @p json at the
 * path of ores.refdata.party_country_junction, the shell command on.
 */
std::vector<std::string> junction_outputs(const std::string& json) {
    const auto g = codegen::ops::load_physical_space(entity_nodes);
    model::types::model_tables t;
    model::ops::builder b(t);
    const auto model = model::io::read_json(b, "{\"junction\": " + json + "}");
    return outputs(codegen::ops::model_units(g, {}, "junction", t, model,
        "projects/ores.refdata/modeling/"
        "ores.refdata.party_country_junction.org", shell_on));
}

bool has(const std::vector<std::string>& outputs, std::string_view o) {
    return std::ranges::find(outputs, o) != outputs.end();
}

}

/*
 * Expected paths are the ones ores.codegen wrote for
 * ores.refdata.party_country_junction.
 */
TEST_CASE("a junction's outputs resolve its name and gate its surfaces",
    "[codegen][physical_space]") {
    const std::string sides = R"("left": {"column": "party_id",
        "list_by": true}, "right": {"column": "country_alpha2_code"})";
    const auto all = junction_outputs(R"({"component": "refdata",
        "subcomponent": "api", "name": "party_countries",
        "name_singular": "party_country", "history": true, )" + sides + "}");
    CHECK(has(all, "projects/ores.refdata/api/include/ores.refdata.api/"
        "domain/party_country.hpp"));
    CHECK(has(all, "projects/ores.refdata/api/include/ores.refdata.api/"
        "messaging/party_country_protocol.hpp"));
    CHECK(has(all, "projects/ores.refdata/core/include/ores.refdata.core/"
        "messaging/party_country_history_provider_registrar.hpp"));
    CHECK(has(all, "projects/ores.refdata/core/include/ores.refdata.core/"
        "presentation/party_country_history_field_mapper.hpp"));
    CHECK(has(all, "projects/ores.shell/refdata/include/ores.shell/app/"
        "commands/refdata/party_country_commands.hpp"));
    CHECK(has(all, "doc/recipes/shell/party_countries/party_country.org"));

    const auto silent = junction_outputs(R"({"component": "refdata",
        "subcomponent": "api", "name": "party_countries",
        "name_singular": "party_country", "left": {"column": "party_id"},
        "right": {"column": "country_alpha2_code"}})");
    CHECK(std::ranges::none_of(silent, [](const std::string& o) {
        return o.contains("history") || o.contains("protocol");
    }));
    CHECK(has(silent, "projects/ores.refdata/api/include/ores.refdata.api/"
        "domain/party_country.hpp"));
}

TEST_CASE("the registry reads operations, plurals and shell opt-ins",
    "[codegen][physical_space]") {
    const codegen::ops::checkout_file files[] = {
        {"projects/ores.workflow/modeling/ores.workflow.workflow_messages.org",
         ":PROPERTIES:\n:END:\n#+title: x\n#+type: ores.codegen.operation\n"
         "#+component: workflow\n#+entity_singular: workflow\n"},
        {"projects/ores.dq/modeling/ores.dq.lei_entity.org",
         "#+title: x\n#+type: ores.codegen.entity\n"
         "#+entity_plural: lei_entities\n#+name: ignored\n"},
        {"projects/ores.refdata/api/modeling/ores.refdata.nested.org",
         "#+entity_plural: nested_things\n"},
        {"projects/ores.shell/trading/modeling/component_overview.org",
         "#+title: x\n#+shell_command_aggregator: true\n"},
        {"projects/ores.shell/iam/modeling/component_overview.org",
         "#+title: x\n#+shell_command_aggregator: false\n"}};
    const auto r = codegen::ops::read_checkout_registry(files);

    CHECK(r.protocol_owners == std::set<std::tuple<std::string, std::string,
        std::string>>{
            {"projects/ores.workflow/modeling", "workflow", "workflow"},
            {"projects/ores.workflow", "workflow", "workflow"}});
    CHECK(r.plural_owners == std::map<std::string, std::set<std::string>,
        std::less<>>{{"lei_entities", {"ores.dq"}}});
    CHECK(r.shell_registrars == std::set<std::string, std::less<>>{
        "trading"});
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

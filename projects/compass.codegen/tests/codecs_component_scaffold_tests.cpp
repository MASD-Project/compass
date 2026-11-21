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
#include <array>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codegen/codecs/codegen.hpp"
#include "compass.cpp/codecs/exported_types.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass;
namespace ct = compass::codec::types;
using codegen::tests::files;
using paths = std::set<std::string>;

/**
 * @brief The node of an archetype of the facet @p facet, for the
 * component kinds @p kinds, writing @p output from @p template_name.
 */
std::string archetype(std::string_view title, std::string_view facet,
    std::string_view kinds, std::string_view output,
    std::string_view template_name) {
    return "#+title: " + std::string(title) + "\n#+type: archetype\n"
        "#+facet: " + std::string(facet) + "\n#+component_kind: " +
        std::string(kinds) + "\n#+output: " + std::string(output) + "\n"
        "#+begin_src mustache :tangle " + std::string(template_name) +
        "\n#+end_src\n";
}

/**
 * @brief An archetype of ORE Studio's ores.cpp component facets:
 * its address under ores.cpp, its kinds, its output under the component
 * directory and its template.
 */
struct fixture_archetype {
    std::string_view facet;
    std::string_view name;
    std::string_view kinds;
    std::string_view output;
    std::string_view template_name;
};

constexpr std::string_view all_kinds = "flat api core service";
constexpr std::string_view library_kinds = "flat api core";

constexpr std::array<fixture_archetype, 19> archetypes{{
    {"component", "export", all_kinds,
     "include/{component_full}/export.hpp", "cpp_export.hpp.mustache"},
    {"component", "test_main", all_kinds, "tests/main.cpp",
     "cpp_component_test_main.cpp.mustache"},
    {"component", "test_suite", all_kinds, "tests/test_suite.hpp",
     "cpp_component_test_suite.hpp.mustache"},
    {"component", "umbrella_header", "flat api core service adapter",
     "include/{component_include_root}/{component_full}.hpp",
     "cpp_component_header.hpp.mustache"},
    {"scaffold", "stub_header", library_kinds,
     "include/{component_full}/domain/stub.hpp",
     "cpp_component_stub_header.hpp.mustache"},
    {"scaffold", "stub_impl", library_kinds, "src/domain/stub.cpp",
     "cpp_component_stub_impl.cpp.mustache"},
    {"scaffold", "stub_test", library_kinds, "tests/stub_tests.cpp",
     "cpp_component_stub_test.cpp.mustache"},
    {"scaffold", "service_stub_test", "service", "tests/stub_tests.cpp",
     "cpp_service_stub_test.cpp.mustache"},
    {"service-app", "application_exception", "service",
     "include/{component_full}/app/application_exception.hpp",
     "cpp_service_app_application_exception.hpp.mustache"},
    {"service-app", "application_header", "service",
     "include/{component_full}/app/application.hpp",
     "cpp_service_app_application.hpp.mustache"},
    {"service-app", "application_impl", "service", "src/app/application.cpp",
     "cpp_service_app_application.cpp.mustache"},
    {"service-app", "config_options_header", "service",
     "include/{component_full}/config/options.hpp",
     "cpp_service_config_options.hpp.mustache"},
    {"service-app", "config_options_impl", "service",
     "src/config/options.cpp", "cpp_service_config_options.cpp.mustache"},
    {"service-app", "config_parser_exception", "service",
     "include/{component_full}/config/parser_exception.hpp",
     "cpp_service_config_parser_exception.hpp.mustache"},
    {"service-app", "config_parser_header", "service",
     "include/{component_full}/config/parser.hpp",
     "cpp_service_config_parser.hpp.mustache"},
    {"service-app", "config_parser_impl", "service", "src/config/parser.cpp",
     "cpp_service_config_parser.cpp.mustache"},
    {"service-app", "host_header", "service",
     "include/{component_full}/app/host.hpp",
     "cpp_service_app_host.hpp.mustache"},
    {"service-app", "host_impl", "service", "src/app/host.cpp",
     "cpp_service_app_host.cpp.mustache"},
    {"service-app", "main", "service", "src/main.cpp",
     "cpp_service_main.cpp.mustache"}}};

/**
 * @brief The export macros header of ORE Studio's ores.cpp.component.export.
 */
constexpr std::string_view export_template =
    "{{#component}}\n"
    "#ifndef {{full_name_upper}}_EXPORT_HPP\n"
    "#define {{full_name_upper}}_EXPORT_HPP\n\n"
    "#include <boost/config.hpp>\n\n"
    "#ifdef {{full_name_upper}}_LIBRARY\n"
    "#  define {{full_name_upper}}_EXPORT BOOST_SYMBOL_EXPORT\n"
    "#else\n"
    "#  define {{full_name_upper}}_EXPORT BOOST_SYMBOL_IMPORT "
    "BOOST_SYMBOL_VISIBLE\n"
    "#endif\n\n"
    "#endif\n"
    "{{/component}}\n";

/**
 * @brief The stub header of ORE Studio's ores.cpp.scaffold.stub_header,
 * as fixed to export its function.
 */
constexpr std::string_view stub_header_template =
    "{{#component}}\n"
    "#ifndef ORES_{{name_upper}}_DOMAIN_STUB_HPP\n"
    "#define ORES_{{name_upper}}_DOMAIN_STUB_HPP\n\n"
    "#include \"{{full_name}}/export.hpp\"\n"
    "#include <string>\n\n"
    "namespace {{namespace}}::domain {\n\n"
    "/**\n"
    " * @brief Stub to be removed once real domain types are added.\n"
    " */\n"
    "struct stub final {\n"
    "    int version = 0;\n"
    "};\n\n"
    "{{full_name_upper}}_EXPORT std::string stub_function();\n\n"
    "}\n\n"
    "#endif\n"
    "{{/component}}\n";

constexpr std::string_view stub_impl_template =
    "{{#component}}\n"
    "#include \"{{full_name}}/domain/stub.hpp\"\n\n"
    "namespace {{namespace}}::domain {\n\n"
    "std::string stub_function() {\n"
    "    return \"STUB code to be removed\";\n"
    "}\n\n"
    "}\n"
    "{{/component}}\n";

/**
 * @brief The template of @p name: ORE Studio's text for those the tests
 * read, a line naming the component for the rest.
 */
std::string template_text(std::string_view name) {
    if (name == "cpp_export.hpp.mustache")
        return std::string(export_template);
    if (name == "cpp_component_stub_header.hpp.mustache")
        return std::string(stub_header_template);
    if (name == "cpp_component_stub_impl.cpp.mustache")
        return std::string(stub_impl_template);
    return "// " + std::string(name) + " {{component.full_name}}\n";
}

/**
 * @brief A library modelled on ORE Studio's ores.cpp component facets:
 * ores.cpp.component and ores.cpp.service-app, enabled by default, and
 * ores.cpp.scaffold, which a component opts into; and a catalogue naming
 * the flat component and the composite one.
 */
files library() {
    files out{
        {"lib/data/licence-GPL-v3.txt", "Licence.\n"},
        {"lib/data/modeline.org", "#+title: Modelines\n"},
        {"lib/component_catalogue.org",
         "* Components\n\n| name | modeling_dir | exclude_org_types |\n"
         "|------+--------------+-------------------|\n"
         "| sample_flat | projects/ores.sample_flat/modeling | |\n"
         "| sample_composite | projects/ores.sample_composite/modeling "
         "| |\n"},
        {"lib/templates/ores.cpp.component.org",
         "#+title: ores.cpp.component\n#+type: facet\n"
         "#+model_types: component\n#+facet_group: ores.cpp\n"},
        {"lib/templates/ores.cpp.service-app.org",
         "#+title: ores.cpp.service-app\n#+type: facet\n"
         "#+model_types: component\n#+facet_group: ores.cpp\n"},
        {"lib/templates/ores.cpp.scaffold.org",
         "#+title: ores.cpp.scaffold\n#+type: facet\n"
         "#+model_types: component\n#+default: disabled\n"
         "#+facet_group: ores.cpp\n"}};
    for (const auto& a : archetypes) {
        const auto facet = "ores.cpp." + std::string(a.facet);
        const auto title = facet + "." + std::string(a.name);
        out.emplace("lib/templates/" + title + ".org", archetype(title, facet,
            a.kinds, "projects/{component_dir}/" + std::string(a.output),
            a.template_name));
        out.emplace("lib/templates/" + std::string(a.template_name),
            template_text(a.template_name));
    }
    return out;
}

/**
 * @brief The overview of a component that opts into the scaffold, as a
 * newly scaffolded one does: @p name under ores., of @p kind, and built
 * with hidden visibility when @p hidden.
 */
std::string overview(std::string_view name, std::string_view kind,
    bool hidden = false) {
    std::string out = ":PROPERTIES:\n:ores.cpp.scaffold.enabled: true\n"
        ":END:\n#+title: ores." + std::string(name) + "\n"
        "#+type: ores.codegen.component\n";
    if (!kind.empty())
        out += "#+component_kind: " + std::string(kind) + "\n";
    if (hidden)
        out += "#+hidden_visibility: true\n";
    return out + "#+name: " + std::string(name) + "\n#+full_name: ores." +
        std::string(name) + "\n#+brief: The " + std::string(name) +
        " fixture.\n";
}

/**
 * @brief The library, a flat component, and a composite one whose group
 * overview holds api, core and service parts; core hides its symbols.
 */
files checkout_files() {
    auto all = library();
    all.emplace("projects/ores.sample_flat/modeling/component_overview.org",
        overview("sample_flat", ""));
    all.emplace(
        "projects/ores.sample_composite/modeling/component_overview.org",
        overview("sample_composite", "composite"));
    for (const auto part : {"api", "core", "service"}) {
        all.emplace("projects/ores.sample_composite/" + std::string(part) +
            "/modeling/component_overview.org",
            overview("sample_composite." + std::string(part), part,
                std::string_view(part) == "core"));
    }
    return all;
}

ct::configuration settings() {
    ct::configuration c;
    c.set("codegen", ct::enabled_key, "true");
    c.set("codegen", "library", "lib/");
    c.set("codegen", "convention_component", "sample_flat");
    c.set("codegen", "year", "2031");
    return c;
}

/**
 * @brief The files the codegen codec writes over a checkout of @p all, by
 * path.
 */
files scaffold(const files& all) {
    const codegen::tests::checkout c(all);
    const std::array codecs{codegen::codecs::codegen};
    const auto r = codec::ops::make_registry(codecs, c.types(), settings());
    REQUIRE(r.problems.empty());
    const auto result = codec::ops::run(r, c.corpus(), c.types(), 1);
    files out;
    const auto& f = result.files[0];
    for (std::uint32_t i = 0; i < f.size(); ++i)
        out.emplace(f.paths[{i}], f.contents[{i}]);
    return out;
}

/**
 * @brief The paths of @p written under the directory @p dir, relative to
 * it.
 */
paths under(const files& written, std::string_view dir) {
    const auto prefix = std::string(dir) + "/";
    paths out;
    for (const auto& [path, text] : written) {
        if (path.starts_with(prefix))
            out.insert(path.substr(prefix.size()));
    }
    return out;
}

/**
 * @brief What every scaffolded library part, of kind flat, api or core,
 * named ores.<name>, is given.
 */
paths library_part(std::string_view name) {
    const auto full = "ores." + std::string(name);
    return {"include/" + full + "/export.hpp",
        "include/" + full + "/" + full + ".hpp",
        "include/" + full + "/domain/stub.hpp",
        "src/domain/stub.cpp",
        "tests/main.cpp",
        "tests/stub_tests.cpp",
        "tests/test_suite.hpp"};
}

}

TEST_CASE("a scaffolded flat component gets its export macros, umbrella "
    "header, test main and suite, and the stubs", "[codegen][scaffold]") {
    const auto written = scaffold(checkout_files());
    CHECK(under(written, "projects/ores.sample_flat") ==
        library_part("sample_flat"));
}

TEST_CASE("a scaffolded composite component writes its parts and nothing "
    "for its group", "[codegen][scaffold]") {
    const auto written = scaffold(checkout_files());
    const std::string root = "projects/ores.sample_composite";
    paths parts;
    for (const auto& p : under(written, root))
        parts.insert(p.substr(0, p.find('/')));
    CHECK(parts == paths{"api", "core", "service"});

    CHECK(under(written, root + "/api") ==
        library_part("sample_composite.api"));
    CHECK(under(written, root + "/core") ==
        library_part("sample_composite.core"));

    const std::string full = "ores.sample_composite.service";
    CHECK(under(written, root + "/service") == paths{
        "include/" + full + "/export.hpp",
        "include/" + full + "/" + full + ".hpp",
        "include/" + full + "/app/application.hpp",
        "include/" + full + "/app/application_exception.hpp",
        "include/" + full + "/app/host.hpp",
        "include/" + full + "/config/options.hpp",
        "include/" + full + "/config/parser.hpp",
        "include/" + full + "/config/parser_exception.hpp",
        "src/app/application.cpp",
        "src/app/host.cpp",
        "src/config/options.cpp",
        "src/config/parser.cpp",
        "src/main.cpp",
        "tests/main.cpp",
        "tests/stub_tests.cpp",
        "tests/test_suite.hpp"});
}

TEST_CASE("a component that does not opt into the scaffold gets no stubs",
    "[codegen][scaffold]") {
    auto all = checkout_files();
    auto& flat =
        all["projects/ores.sample_flat/modeling/component_overview.org"];
    flat.erase(0, flat.find("#+title"));
    const auto full = std::string("ores.sample_flat");
    CHECK(under(scaffold(all), "projects/ores.sample_flat") == paths{
        "include/" + full + "/export.hpp",
        "include/" + full + "/" + full + ".hpp",
        "tests/main.cpp",
        "tests/test_suite.hpp"});
}

TEST_CASE("a hidden-visibility part's stub header exports its function, "
    "which the exported-types check accepts", "[codegen][scaffold]") {
    auto all = checkout_files();
    const std::string core = "projects/ores.sample_composite/core";
    const auto written = scaffold(all);
    const auto& header = written.at(core +
        "/include/ores.sample_composite.core/domain/stub.hpp");
    CHECK(header.find(
        "#include \"ores.sample_composite.core/export.hpp\"\n") !=
        std::string::npos);
    CHECK(header.find("ORES_SAMPLE_COMPOSITE_CORE_EXPORT std::string "
        "stub_function();\n") != std::string::npos);

    for (const auto& [path, text] : written)
        all.insert_or_assign(path, text);
    const codegen::tests::checkout c(all);
    ct::configuration enabled;
    enabled.set("exported_types", ct::enabled_key, "true");
    const std::array codecs{cpp::codecs::exported_types};
    const auto run_check = [&](const codegen::tests::checkout& over) {
        const auto r = codec::ops::make_registry(codecs, over.types(),
            enabled);
        return codec::ops::run(r, over.corpus(), over.types(), 1)
            .diagnostics[0].size();
    };
    CHECK(run_check(c) == 0);

    auto unexported = all;
    auto& text = unexported.at(core +
        "/include/ores.sample_composite.core/domain/stub.hpp");
    const std::string macro = "ORES_SAMPLE_COMPOSITE_CORE_EXPORT ";
    text.erase(text.find(macro), macro.size());
    CHECK(run_check(codegen::tests::checkout(unexported)) == 1);
}

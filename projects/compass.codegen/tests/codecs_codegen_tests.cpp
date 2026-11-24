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
#include "compass.codegen/codecs/codegen.hpp"

#include <array>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/ops/outputs.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass;
namespace ct = compass::codec::types;
using codegen::tests::checkout;
using codegen::tests::files;
using counts = std::vector<std::uint64_t>;

/**
 * @brief A library with one facet of component models and its one
 * archetype, the component model it generates for, a catalogue naming the
 * component and a document nobody reads.
 */
files library() {
    return {
        {"lib/data/licence-GPL-v3.txt", "Licence.\n"},
        {"lib/data/modeline.org", "#+title: Modelines\n"},
        {"lib/component_catalogue.org",
         "* Components\n\n| name | modeling_dir | exclude_org_types |\n"
         "|------+--------------+-------------------|\n"
         "| a | projects/ores.a/modeling | |\n"},
        {"lib/templates/ores.cmake.component.org",
         "#+title: ores.cmake.component\n#+type: facet\n"
         "#+facet_group: ores.cmake\n#+model_types: component\n"},
        {"lib/templates/ores.cmake.component.root.org",
         "#+title: ores.cmake.component.root\n#+type: archetype\n"
         "#+facet: ores.cmake.component\n#+component_kind: flat\n"
         "#+output: projects/{component_dir}/CMakeLists.txt\n"
         "#+begin_src mustache :tangle root.mustache\n#+end_src\n"},
        {"lib/templates/root.mustache",
         "project({{component.full_name}})\n"},
        {"projects/ores.a/modeling/component_overview.org",
         "#+title: ores.a\n#+type: ores.codegen.component\n#+name: a\n"
         "#+full_name: ores.a\n#+brief: A\n#+component_kind: flat\n"},
        {"doc/notes.org", "#+title: Notes\n"},
        {"src/main.cpp", "int main() {}\n"}};
}

ct::configuration settings() {
    ct::configuration c;
    c.set("codegen", ct::enabled_key, "true");
    c.set("codegen", "library", "lib/");
    c.set("codegen", "product", "ores");
    c.set("codegen", "convention_component", "a");
    c.set("codegen", "year", "2031");
    return c;
}

struct outcome {
    ct::run_result run;
    std::string text;
};

std::vector<std::pair<std::string, std::string>> rows_of(
    const ct::file_table& t) {
    std::vector<std::pair<std::string, std::string>> out;
    for (std::uint32_t f = 0; f < t.size(); ++f)
        out.emplace_back(t.paths[{f}], t.contents[{f}]);
    return out;
}

files by_path(const ct::file_table& t) {
    files out;
    for (auto& [path, text] : rows_of(t))
        out.emplace(std::move(path), std::move(text));
    return out;
}

/**
 * @brief Runs codegen over @p all from @p previous, and checks that its
 * files are those of a run from nothing, which generates every model.
 */
outcome run(const files& all, const ct::dependency_record& previous = {}) {
    const checkout c(all);
    const std::array codecs{codegen::codecs::codegen};
    const auto r = codec::ops::make_registry(codecs, c.types(), settings());
    REQUIRE(r.problems.empty());
    auto result = codec::ops::run(r, c.corpus(), c.types(), 1, previous);
    const auto whole = codec::ops::run(r, c.corpus(), c.types(), 1);
    CHECK(rows_of(result.files[0]) == rows_of(whole.files[0]));
    std::string text;
    if (result.files[0].size() == 1)
        text = result.files[0].contents[{0}];
    return {std::move(result), text};
}

/**
 * @brief The models of @p r that were generated, by path, and "" when
 * what they share was read again.
 */
std::set<std::string> generated(const ct::run_result& r) {
    std::set<std::string> out;
    for (const auto u : r.fresh)
        out.emplace(r.record.documents[u]);
    return out;
}

constexpr std::string_view model_a =
    "projects/ores.a/modeling/component_overview.org";

constexpr std::string_view registry_model =
    "projects/modeling/service_registry.org";

/**
 * @brief library() with a second archetype of the component, whose
 * template lists the convention families, and an entity model of the
 * convention component that declares one. The catalogue leaves entity
 * models out of generation, so the entity is read for the registry alone.
 */
files library_with_family() {
    auto all = library();
    all["lib/component_catalogue.org"] =
        "* Components\n\n| name | modeling_dir | exclude_org_types |\n"
        "|------+--------------+-------------------|\n"
        "| a | projects/ores.a/modeling | entity |\n";
    all["lib/templates/ores.cmake.component.families.org"] =
        "#+title: ores.cmake.component.families\n#+type: archetype\n"
        "#+facet: ores.cmake.component\n#+component_kind: flat\n"
        "#+output: projects/{component_dir}/families.ts\n"
        "#+begin_src mustache :tangle "
        "ts_convention_family_registry.ts.mustache\n#+end_src\n";
    all["lib/templates/ts_convention_family_registry.ts.mustache"] =
        "{{#convention_registry.families}}{{key}};"
        "{{/convention_registry.families}}\n";
    all["projects/ores.a/modeling/ores.a.swap.org"] =
        "#+title: Swap\n#+type: ores.codegen.entity\n"
        "#+convention_family: swaps\n#+entity_singular: swap\n";
    return all;
}

/**
 * @brief library() with the two facets of the service registry, three of
 * their archetypes, and a registry in projects/modeling, which no
 * catalogue entry names: two services with a psql variable, one with an
 * apostrophe in its display name, and a disabled one without.
 */
files library_with_registry() {
    auto all = library();
    all["lib/templates/ores.sql.service.org"] =
        "#+title: ores.sql.service\n#+type: facet\n"
        "#+facet_group: ores.sql\n#+model_types: service_registry\n";
    all["lib/templates/ores.shell.service.org"] =
        "#+title: ores.shell.service\n#+type: facet\n"
        "#+facet_group: ores.shell\n#+model_types: service_registry\n";
    all["lib/templates/ores.sql.service.db_grants.org"] =
        "#+title: ores.sql.service.db_grants\n#+type: archetype\n"
        "#+facet: ores.sql.service\n"
        "#+output: projects/ores.sql/create/grants.sql\n"
        "#+begin_src mustache :tangle grants.mustache\n#+end_src\n";
    all["lib/templates/grants.mustache"] =
        "to\n"
        "{{#service_registry_db_services}}\n"
        "    :{{psql_var}}_user{{^last}},{{/last}}{{#last}};{{/last}}\n"
        "{{/service_registry_db_services}}\n"
        "{{#service_registry_db_services}}\n"
        "{{#dml_prefixes}}\n"
        "select dml('{{prefix}}', :'{{psql_var}}_user');\n"
        "{{/dml_prefixes}}\n"
        "{{#select_tables}}\n"
        "grant select on {{table}} to :{{psql_var}}_user;\n"
        "{{/select_tables}}\n"
        "{{/service_registry_db_services}}\n";
    all["lib/templates/ores.sql.service.expected.org"] =
        "#+title: ores.sql.service.expected\n#+type: archetype\n"
        "#+facet: ores.sql.service\n"
        "#+output: projects/ores.sql/populate/expected.sql\n"
        "#+begin_src mustache :tangle expected.mustache\n#+end_src\n";
    all["lib/templates/expected.mustache"] =
        "{{#service_registry.services}}\n"
        "{{#enabled}}\n"
        "('{{name}}', {{replicas}}, '{{display_name_sql}}', "
        "'{{summary_sql}}', {{#account_var}}:'{{account_var}}'"
        "{{/account_var}}{{^account_var}}null{{/account_var}}, "
        "'{{image_code}}')\n"
        "{{/enabled}}\n"
        "{{/service_registry.services}}\n";
    all["lib/templates/ores.shell.service.vars.org"] =
        "#+title: ores.shell.service.vars\n#+type: archetype\n"
        "#+facet: ores.shell.service\n"
        "#+output: projects/ores.sql/service_vars.sh\n"
        "#+begin_src mustache :tangle vars.mustache\n#+end_src\n";
    all["lib/templates/vars.mustache"] =
        "SERVICE_NAMES=(\n"
        "{{#service_registry_db_services}}\n"
        "    {{component}}\n"
        "{{/service_registry_db_services}}\n"
        ")\n";
    all[std::string(registry_model)] =
        "#+title: Service registry\n"
        "#+type: ores.codegen.service_registry\n\n"
        "* ores.iam.service\n:PROPERTIES:\n:psql_var: iam_service\n"
        ":display_name: Identity\n:summary: Signs people in.\n"
        ":replicas: 2\n:END:\n\n"
        "** DML prefixes\n\n- ores_iam_\n\n"
        "** Select tables\n\n- table=ores_refdata_x_tbl\n\n"
        "* ores.wt.service\n:PROPERTIES:\n:psql_var: wt\n"
        ":display_name: Walt's\n:summary: It's a test.\n"
        ":replicas: 1\n:END:\n\n"
        "** DML prefixes\n\n- ores_wt_\n\n"
        "* ores.fleet\n:PROPERTIES:\n:replicas: 1\n:enabled: false\n"
        ":END:\n";
    return all;
}

/**
 * @brief A directory with an executable named fake-format in bin/.
 */
class scoped_bin final {
    compass::testing::filesystem::scoped_directory dir_{"compass.codegen.bin"};

public:
    scoped_bin() {
        std::filesystem::create_directories(dir_.path() / "bin");
        const auto program = dir_.path() / "bin/fake-format";
        std::ofstream(program) << "#!/bin/sh\n";
        std::filesystem::permissions(program,
            std::filesystem::perms::owner_all);
    }

    [[nodiscard]] const std::filesystem::path& root() const {
        return dir_.path();
    }
};

/**
 * @brief The options from @p values, in the order of the codec's options
 * but for codec_component, which @p codec_component gives.
 */
ct::option_values values_of(const std::array<std::string, 8>& values,
    std::string codec_component = {}) {
    // option_values views its values, so each list outlives the test.
    static std::deque<std::vector<std::string>> kept;
    auto& all = kept.emplace_back(values.begin(), values.begin() + 3);
    all.push_back(std::move(codec_component));
    all.insert(all.end(), values.begin() + 3, values.end());
    return {codegen::codecs::codegen.options, all};
}

}

TEST_CASE("the codegen codec renders each output the library's graph "
    "gives a model", "[codegen][codec]") {
    const auto r = run(library());
    REQUIRE(r.run.files[0].size() == 1);
    CHECK(r.run.files[0].paths[{0}] == "projects/ores.a/CMakeLists.txt");
    CHECK(r.text.find("project(ores.a)") != std::string::npos);
}

TEST_CASE("many outputs, rendered on many threads, each hold their own "
    "model's text, in the order the models are planned", "[codegen][codec]") {
    auto all = library();
    std::string catalogue = "* Components\n\n"
        "| name | modeling_dir | exclude_org_types |\n"
        "|------+--------------+-------------------|\n";
    std::vector<std::string> names{"a"};
    for (int i = 100; i < 400; ++i)
        names.push_back("c" + std::to_string(i));
    for (const auto& n : names) {
        catalogue += "| " + n + " | projects/ores." + n + "/modeling | |\n";
        all["projects/ores." + n + "/modeling/component_overview.org"] =
            "#+title: ores." + n + "\n#+type: ores.codegen.component\n"
            "#+name: " + n + "\n#+full_name: ores." + n + "\n#+brief: B\n"
            "#+component_kind: flat\n";
    }
    all["lib/component_catalogue.org"] = catalogue;

    const auto r = run(all);
    const auto& out = r.run.files[0];
    REQUIRE(out.size() == names.size());
    for (std::uint32_t i = 0; i < out.size(); ++i) {
        INFO(names[i]);
        CHECK(out.paths[{i}] == "projects/ores." + names[i] +
            "/CMakeLists.txt");
        CHECK(out.contents[{i}] == "project(ores." + names[i] + ")\n");
    }
}

TEST_CASE("the codegen codec runs again for a model or template edit and "
    "not for another document or file", "[codegen][codec]") {
    const auto first = run(library());

    auto other = library();
    other["doc/notes.org"] += "More notes.\n";
    other["src/main.cpp"] += "// more\n";
    const auto unrelated = run(other, first.run.record);
    CHECK(unrelated.run.ran == counts{0});
    CHECK(unrelated.run.skipped == counts{2});

    auto model = library();
    model["projects/ores.a/modeling/component_overview.org"] =
        "#+title: ores.a\n#+type: ores.codegen.component\n#+name: a\n"
        "#+full_name: ores.b\n#+brief: A\n#+component_kind: flat\n";
    const auto remodelled = run(model, first.run.record);
    CHECK(generated(remodelled.run) ==
        std::set<std::string>{"", std::string(model_a)});
    CHECK(remodelled.text.find("project(ores.b)") != std::string::npos);

    auto edited = library();
    edited["lib/templates/root.mustache"] = "name({{component.name}})\n";
    const auto retemplated = run(edited, first.run.record);
    CHECK(generated(retemplated.run) ==
        std::set<std::string>{"", std::string(model_a)});
    CHECK(retemplated.text.find("name(a)") != std::string::npos);
}

TEST_CASE("a document that appears in a modeling directory is read for "
    "what the models share, and generates no model it changes nothing for",
    "[codegen][codec]") {
    const auto first = run(library());
    auto added = library();
    added["projects/ores.a/modeling/notes.org"] = "#+title: More\n";

    CHECK(generated(run(added, first.run.record).run) ==
        std::set<std::string>{""});
}

TEST_CASE("a model is generated again when a model it never read changes "
    "an element of the registry it looked up", "[codegen][codec]") {
    const auto first = run(library_with_family());
    REQUIRE(rows_of(first.run.files[0])[0] ==
        std::pair<std::string, std::string>{"projects/ores.a/families.ts",
            "swaps;\n"});
    auto all = library_with_family();
    auto& entity = all["projects/ores.a/modeling/ores.a.swap.org"];
    entity.replace(entity.find("swaps"), 5, "basis");

    const auto second = run(all, first.run.record);

    CHECK(generated(second.run) ==
        std::set<std::string>{"", std::string(model_a)});
    CHECK(rows_of(second.run.files[0])[0].second == "basis;\n");
}

TEST_CASE("a model is not generated again when a model of the registry "
    "changes nothing it looked up", "[codegen][codec]") {
    const auto first = run(library_with_family());
    auto all = library_with_family();
    all["projects/ores.a/modeling/ores.a.swap.org"] += "A note.\n";

    const auto second = run(all, first.run.record);

    CHECK(generated(second.run) == std::set<std::string>{""});
    CHECK(second.run.skipped == counts{1});
}

TEST_CASE("a template edit renders that template's outputs again and "
    "keeps the model's others", "[codegen][codec]") {
    const auto first = run(library_with_family());
    auto all = library_with_family();
    all["lib/templates/root.mustache"] = "name({{component.name}})\n";

    const auto second = run(all, first.run.record);

    CHECK(generated(second.run) ==
        std::set<std::string>{"", std::string(model_a)});
    CHECK(rows_of(second.run.files[0]) ==
        std::vector<std::pair<std::string, std::string>>{
            {"projects/ores.a/families.ts", "swaps;\n"},
            {"projects/ores.a/CMakeLists.txt", "name(a)\n"}});

    auto& entity = all["projects/ores.a/modeling/ores.a.swap.org"];
    entity.replace(entity.find("swaps"), 5, "basis");
    const auto third = run(all, second.run.record);
    CHECK(rows_of(third.run.files[0])[0].second == "basis;\n");
}

TEST_CASE("an output two models name is written once, from the later",
    "[codegen][codec]") {
    auto all = library();
    all["lib/templates/ores.cmake.component.root.org"] =
        "#+title: ores.cmake.component.root\n#+type: archetype\n"
        "#+facet: ores.cmake.component\n#+component_kind: flat\n"
        "#+output: projects/CMakeLists.txt\n"
        "#+begin_src mustache :tangle root.mustache\n#+end_src\n";
    all["projects/ores.b/modeling/component_overview.org"] =
        "#+title: ores.b\n#+type: ores.codegen.component\n#+name: b\n"
        "#+full_name: ores.b\n#+brief: B\n#+component_kind: flat\n";
    all["lib/component_catalogue.org"] =
        "* Components\n\n| name | modeling_dir | exclude_org_types |\n"
        "|------+--------------+-------------------|\n"
        "| a | projects/ores.a/modeling | |\n"
        "| b | projects/ores.b/modeling | |\n";

    const auto r = run(all);

    REQUIRE(r.run.files[0].size() == 1);
    CHECK(r.text.find("project(ores.b)") != std::string::npos);

    const checkout c(all);
    const std::array codecs{codegen::codecs::codegen};
    const auto registry = codec::ops::make_registry(codecs, c.types(),
        settings());
    const auto outputs = codec::ops::outputs_of(registry, r.run);
    REQUIRE(outputs.size() == 1);
    CHECK(codec::ops::check_outputs(outputs).empty());

    all["projects/ores.a/modeling/component_overview.org"] += "A note.\n";
    const auto edited = run(all, r.run.record);
    REQUIRE(edited.run.files[0].size() == 1);
    CHECK(edited.text.find("project(ores.b)") != std::string::npos);
}

TEST_CASE("the service registry, which no catalogue entry names, renders "
    "the outputs of its facets", "[codegen][codec]") {
    const auto r = run(library_with_registry());

    CHECK(by_path(r.run.files[0]) == files{
            {"projects/ores.a/CMakeLists.txt", "project(ores.a)\n"},
            {"projects/ores.sql/service_vars.sh",
             "SERVICE_NAMES=(\n    iam\n    wt\n)\n"},
            {"projects/ores.sql/create/grants.sql",
             "to\n    :iam_service_user,\n    :wt_user;\n"
             "select dml('ores_iam_', :'iam_service_user');\n"
             "grant select on ores_refdata_x_tbl to :iam_service_user;\n"
             "select dml('ores_wt_', :'wt_user');\n"},
            {"projects/ores.sql/populate/expected.sql",
             "('ores.iam.service', 2, 'Identity', 'Signs people in.', "
             ":'iam_service_user', 'ores_iam_service')\n"
             "('ores.wt.service', 1, 'Walt''s', 'It''s a test.', "
             ":'wt_user', 'ores_wt_service')\n"}});
}

TEST_CASE("a service added to the registry reaches every output of the "
    "registry", "[codegen][codec]") {
    const auto first = run(library_with_registry());
    auto all = library_with_registry();
    all[std::string(registry_model)] +=
        "\n* ores.new.service\n:PROPERTIES:\n:psql_var: new_service\n"
        ":account_var: new_account\n:replicas: 3\n:END:\n\n"
        "** DML prefixes\n\n- ores_new_\n";

    const auto second = run(all, first.run.record);

    CHECK(generated(second.run) ==
        std::set<std::string>{"", std::string(registry_model)});
    auto rows = by_path(second.run.files[0]);
    REQUIRE(rows.size() == 4);
    CHECK(rows["projects/ores.sql/service_vars.sh"] ==
        "SERVICE_NAMES=(\n    iam\n    wt\n    new\n)\n");
    CHECK(rows["projects/ores.sql/create/grants.sql"] ==
        "to\n    :iam_service_user,\n    :wt_user,\n"
        "    :new_service_user;\n"
        "select dml('ores_iam_', :'iam_service_user');\n"
        "grant select on ores_refdata_x_tbl to :iam_service_user;\n"
        "select dml('ores_wt_', :'wt_user');\n"
        "select dml('ores_new_', :'new_service_user');\n");
    CHECK(rows["projects/ores.sql/populate/expected.sql"].ends_with(
        "('ores.new.service', 3, '', '', :'new_account', "
        "'ores_new_service')\n"));
}

TEST_CASE("the codegen codec needs its library, product, convention "
    "component and year", "[codegen][codec]") {
    CHECK_THROWS_WITH(codegen::codecs::settings_of(values_of(
        {"lib", "", "refdata", "2031", "", "", "", ""})),
        "codegen: set the option product");
    CHECK_THROWS_AS(codegen::codecs::settings_of(values_of(
        {"", "ores", "refdata", "", "", "", "", ""})), std::runtime_error);
    CHECK_THROWS_AS(codegen::codecs::settings_of(values_of(
        {"lib", "ores", "", "", "", "", "", ""})), std::runtime_error);
    CHECK_THROWS_AS(codegen::codecs::settings_of(values_of(
        {"lib", "ores", "refdata", "20x6", "", "", "", ""})),
        std::runtime_error);
    CHECK_THROWS_WITH(codegen::codecs::settings_of(values_of(
        {"lib", "ores", "refdata", "", "", "", "", ""})),
        "codegen: set the option year, the copyright year of the licence "
        "blocks");
    const auto s = codegen::codecs::settings_of(values_of(
        {"lib//", "ores", "refdata", "2031", "", "", "", ""}));
    CHECK(s.libraries == std::vector<std::string>{"lib"});
    CHECK(s.product == "ores");
    CHECK(s.year == 2031);
    CHECK(s.codec_component.empty());
    CHECK(codegen::codecs::settings_of(values_of(
        {"lib", "ores", "refdata", "2031", "", "", "", ""}, "db"))
              .codec_component == "db");
}

TEST_CASE("the formatters are ores.codegen's, with the programs and styles "
    "the options name", "[codegen][codec]") {
    const scoped_bin bin;
    CHECK(codegen::codecs::formatters_of(values_of(
        {"lib", "ores", "a", "", "", "", "", ""}), bin.root()).empty());

    const auto both = codegen::codecs::formatters_of(values_of(
        {"lib", "ores", "a", "", "missing/clang-format,bin/fake-format",
         ".clang-format", "bin/fake-format", ".prettierrc.yaml"}),
        bin.root());
    REQUIRE(both.size() == 2);
    CHECK(both[0].name == "clang-format");
    CHECK(both[0].program == bin.root() / "bin/fake-format");
    CHECK(both[0].passes == 2);
    CHECK(both[0].style == bin.root() / ".clang-format");
    CHECK(both[1].name == "prettier");
    CHECK(both[1].extensions == std::vector<std::string>{".ts", ".tsx"});
    CHECK(both[1].style == bin.root() / ".prettierrc.yaml");

    CHECK_THROWS_AS(codegen::codecs::formatters_of(values_of(
        {"lib", "ores", "a", "", "missing/clang-format", ".clang-format", "",
         ""}),
        bin.root()), std::runtime_error);
    CHECK_THROWS_AS(codegen::codecs::formatters_of(values_of(
        {"lib", "ores", "a", "", "bin/fake-format", "", "", ""}), bin.root()),
        std::runtime_error);
}

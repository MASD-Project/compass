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
#include "compass.codegen/ops/library.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codegen/codecs/codegen.hpp"
#include "compass.platform/process/executable.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass;
namespace ct = compass::codec::types;
namespace fs = std::filesystem;
using codegen::tests::checkout;
using codegen::tests::files;

/**
 * @brief The project's half of a library: its data, its catalogue and the
 * one facet and archetype of component models, whose template it leaves
 * to the library below it.
 */
files project() {
    return {
        {"lib/data/licence-GPL-v3.txt", "Project licence.\n"},
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
        {"projects/ores.a/modeling/component_overview.org",
         "#+title: ores.a\n#+type: ores.codegen.component\n#+name: a\n"
         "#+full_name: ores.a\n#+brief: A\n#+component_kind: flat\n"}};
}

/**
 * @brief A library outside the checkout, as compass installs one.
 */
class shared_library final {
    compass::testing::filesystem::scoped_directory dir_{
        "compass.codegen.library"};

public:
    void write(const std::string& path, const std::string& text) {
        const auto file = dir_.path() / path;
        fs::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << text;
    }

    [[nodiscard]] std::string path() const {
        return dir_.path().generic_string();
    }
};

/**
 * @brief What codegen writes for the checkout @p all with the option
 * library @p library.
 */
std::string generate(const files& all, const std::string& library) {
    const checkout c(all);
    ct::configuration settings;
    settings.set("codegen", ct::enabled_key, "true");
    settings.set("codegen", "library", library);
    settings.set("codegen", "product", "ores");
    settings.set("codegen", "convention_component", "a");
    settings.set("codegen", "year", "2031");
    const std::array codecs{codegen::codecs::codegen};
    const auto r = codec::ops::make_registry(codecs, c.types(), settings);
    REQUIRE(r.problems.empty());
    const auto result = codec::ops::run(r, c.corpus(), c.types(), 1);
    REQUIRE(result.files[0].size() == 1);
    return std::string(result.files[0].contents[{0}]);
}

}

TEST_CASE("@compass is share/compass/masd beside the binary's directory",
    "[codegen][library]") {
    CHECK(codegen::ops::compass_library_directory("/opt/compass/bin/compass")
        .generic_string() == "/opt/compass/share/compass/masd");
    CHECK(codegen::ops::compass_library_directory(
        "C:/Program Files/compass/bin/compass.exe").generic_string() ==
        "C:/Program Files/compass/share/compass/masd");
}

TEST_CASE("the library option lists directories in order",
    "[codegen][library]") {
    CHECK(codegen::ops::library_directories(
        "@compass, projects/ores.codegen/library/",
        "/home/u/.local/opt/compass/bin/compass") ==
        std::vector<std::string>{
            "/home/u/.local/opt/compass/share/compass/masd",
            "projects/ores.codegen/library"});
    CHECK(codegen::ops::library_directories("lib", {}) ==
        std::vector<std::string>{"lib"});
    CHECK_THROWS_WITH(codegen::ops::library_directories("@compass,,lib",
        "/opt/compass/bin/compass"),
        "codegen: the option library names an empty directory");
    CHECK_THROWS_AS(codegen::ops::library_directories("@compass", {}),
        std::runtime_error);
    CHECK(codegen::ops::outside_the_checkout(
        codegen::ops::compass_library_directory(
            platform::process::executable_path()).generic_string()));
    CHECK_FALSE(codegen::ops::outside_the_checkout("lib"));
}

TEST_CASE("the build stages the MASD library where @compass finds it",
    "[codegen][library]") {
    const auto directories = codegen::ops::library_directories("@compass",
        platform::process::executable_path());
    REQUIRE(directories.size() == 1);
    CHECK(fs::is_regular_file(fs::path(directories[0]) /
        "templates/cmake_component_root.mustache"));
}

TEST_CASE("a template of a later library replaces the earlier one's",
    "[codegen][library]") {
    shared_library masd;
    masd.write("templates/root.mustache", "masd({{component.full_name}})\n");
    masd.write("data/licence-GPL-v3.txt", "MASD licence.\n");
    const auto layered = masd.path() + ",lib";

    CHECK(generate(project(), layered) == "masd(ores.a)\n");

    auto own = project();
    own.emplace("lib/templates/root.mustache",
        "{{{licence-GPL-v3}}}project({{component.full_name}})\n");
    const auto text = generate(own, layered);
    CHECK(text.ends_with("project(ores.a)\n"));
    CHECK(text.contains("Project licence."));
    CHECK_FALSE(text.contains("MASD licence."));
}

TEST_CASE("a node two libraries hold is an error", "[codegen][library]") {
    shared_library masd;
    masd.write("templates/root.mustache", "masd\n");
    masd.write("templates/ores.cmake.component.org",
        "#+title: ores.cmake.component\n#+type: facet\n");
    CHECK_THROWS_WITH(generate(project(), masd.path() + ",lib"),
        Catch::Matchers::ContainsSubstring("both hold the node "
            "ores.cmake.component.org"));
}

TEST_CASE("a library outside the checkout keys the codec's configuration",
    "[codegen][library]") {
    shared_library masd;
    masd.write("templates/root.mustache", "masd\n");
    const std::array<std::string, 8> values{masd.path() + ",lib", "ores", "a",
        "2031", "", "", "", ""};
    const ct::option_values options{codegen::codecs::codegen.options, values};
    const auto environment = codegen::codecs::codegen.environment;
    REQUIRE(environment != nullptr);
    const auto before = environment(options);
    masd.write("templates/root.mustache", "masd, edited\n");
    CHECK(environment(options) != before);

    const std::array<std::string, 8> inside{"lib", "ores", "a", "2031", "",
        "", "", ""};
    CHECK(environment({codegen::codecs::codegen.options, inside}).empty());
}

TEST_CASE("a library outside the checkout that is not a directory is "
    "refused", "[codegen][library]") {
    const shared_library masd;
    const auto missing = masd.path() + "/missing";
    const std::array<std::string, 8> values{missing, "ores", "a", "2031", "",
        "", "", ""};
    CHECK_THROWS_WITH(codegen::codecs::settings_of(
        {codegen::codecs::codegen.options, values},
        platform::process::executable_path()),
        "codegen: the library " + missing + " is not a directory");
}

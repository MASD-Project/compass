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
#include "compass.codegen/codecs/component_files.hpp"

#include <array>
#include <map>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass;
namespace ct = compass::codec::types;
using files = std::map<std::string, std::string, std::less<>>;

/**
 * @brief Runs component_files alone over a checkout of @p all, none of it
 * a document, with the options year @p year and library @p library, and
 * returns what it writes.
 */
files generate(const files& all, std::string_view year = "2026",
    std::string_view library = "projects/ores.codegen/library") {
    ct::checkout_files checkout;
    for (const auto& [path, bytes] : all)
        checkout.paths.push_back(path);
    checkout.content_of = [&](std::string_view p) {
        return std::optional(store::ops::blob_id_of(all.find(p)->second));
    };
    checkout.read = [&](std::string_view p) {
        return std::optional(all.find(p)->second);
    };
    ct::corpus corpus;
    corpus.files = &checkout;
    const org::types::type_registry types;
    ct::configuration configuration;
    configuration.set("component_files", "enabled", "true");
    configuration.set("component_files", "year", year);
    configuration.set("component_files", "library", library);
    const std::array codecs{codegen::codecs::component_files};
    const auto r = codec::ops::run(codec::ops::make_registry(codecs, types,
        configuration), corpus, types, 2);
    files out;
    const auto& f = r.files[0];
    for (std::uint32_t i = 0; i < f.size(); ++i)
        out.emplace(f.paths[ct::file_id{i}], f.contents[ct::file_id{i}]);
    return out;
}

const std::string_view lib = "projects/ores.codegen/library/";

files tree() {
    const std::string l(lib);
    return {
        {l + "data/licence-GPL-v3.txt", "Licence.\n"},
        {l + "data/modeline.org",
         "*** ores.modelines.emacs.cmake :modeline:\n"
         ":masd.codec.value: mode: cmake\n"},
        {l + "templates/cmake_component_files_src.mustache",
         "{{{cmake_license}}}\n{{#component}}\nset(files\n{{#files}}\n"
         "    \"{{.}}\"\n{{/files}}\n)\n{{#has_headers}}\nH {{headers}}\n"
         "{{/has_headers}}\n{{/component}}\n"},
        {l + "templates/cmake_component_files_tests.mustache",
         "tests{{#component}} {{files}}{{/component}}\n"},
        {"projects/ores.a/modeling/component_overview.org",
         "#+title: a\n#+full_name: ores.a\n"},
        {"projects/ores.a/src/CMakeLists.txt", ""},
        {"projects/ores.a/src/z.cpp", ""},
        {"projects/ores.a/src/dir/b.cpp", ""},
        {"projects/ores.a/src/b.hpp", ""},
        {"projects/ores.a/include/ores.a/x.hpp", ""},
        {"projects/ores.b/modeling/component_overview.org",
         "#+full_name: ores.b\n"},
        {"projects/ores.b/src/b.cpp", ""},
        {"projects/ores.b/tests/CMakeLists.txt", ""},
        {"projects/ores.b/tests/t.cpp", ""},
        {"projects/ores.c/modeling/component_overview.org", "no name\n"},
        {"projects/ores.c/src/CMakeLists.txt", ""}};
}

}

TEST_CASE("each CMake directory of a modelled component lists its sources "
    "and its component's headers", "[codecs][component_files]") {
    const auto out = generate(tree());

    REQUIRE(out.size() == 2);
    CHECK(out.at("projects/ores.a/src/component_files.cmake") ==
        "# -*- mode: cmake -*-\n#\n"
        "# Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>\n"
        "#\n# Licence.\n#\n"
        "# AUTO-GENERATED FILE - DO NOT EDIT MANUALLY\n"
        "# Template: cmake_component_files_src.mustache\n"
        "# To modify, update the template and regenerate.\n"
        "set(files\n    \"dir/b.cpp\"\n    \"z.cpp\"\n)\n"
        "H ['ores.a/x.hpp']\n");
    CHECK(out.at("projects/ores.b/tests/component_files.cmake") ==
        "tests ['t.cpp']\n");
}

TEST_CASE("the copyright year is the configured one, and an empty year is "
    "refused rather than taken from the clock", "[codecs][component_files]") {
    CHECK(generate(tree(), "2031").at("projects/ores.a/src/"
        "component_files.cmake").find("# Copyright (C) 2031 Marco") !=
        std::string::npos);
    CHECK_THROWS_WITH(generate(tree(), ""), "component_files: set the option "
        "year, the copyright year of the licence blocks");
}

TEST_CASE("the library is configuration, with no default",
    "[codecs][component_files]") {
    CHECK_THROWS_WITH(generate(tree(), "2031", ""),
        "component_files: set the option library");
}

TEST_CASE("a component with no include directory lists no headers",
    "[codecs][component_files]") {
    auto all = tree();
    all.erase("projects/ores.a/include/ores.a/x.hpp");

    const auto out = generate(all);

    CHECK(out.at("projects/ores.a/src/component_files.cmake").ends_with(
        "    \"z.cpp\"\n)\n"));
}

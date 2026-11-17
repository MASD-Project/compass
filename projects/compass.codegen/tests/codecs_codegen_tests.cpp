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
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.ingest/meta/type_registry_builder.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass;
namespace ct = compass::codec::types;
using files = std::map<std::string, std::string, std::less<>>;
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

/**
 * @brief A checkout of @p all: its .org files parsed into a corpus typed
 * by the model types, and every file listed.
 */
class checkout final {
public:
    explicit checkout(const files& all) : bytes_(all) {
        ingest::meta::type_registry_builder builder;
        for (const auto name : ingest::meta::model_types)
            builder.add_model(name);
        types_ = builder.build();
        for (const auto& [path, text] : bytes_) {
            files_.paths.push_back(path);
            if (!path.ends_with(".org"))
                continue;
            auto& t = tables_.emplace_back();
            ingest::org::parse(text, t);
            const auto view = org::types::view(t);
            corpus_.paths.push_back(path);
            corpus_.contents.push_back(store::ops::blob_id_of(text));
            corpus_.sources.push_back(text);
            corpus_.tables.push_back(view);
            corpus_.types.push_back(codec::ops::document_type(text, view,
                types_));
        }
        files_.content_of = [this](std::string_view p) {
            return std::optional(store::ops::blob_id_of(
                bytes_.find(p)->second));
        };
        files_.read = [this](std::string_view p) {
            return std::optional(bytes_.find(p)->second);
        };
        corpus_.files = &files_;
    }
    checkout(const checkout&) = delete;
    checkout& operator=(const checkout&) = delete;

    [[nodiscard]] const ct::corpus& corpus() const { return corpus_; }
    [[nodiscard]] const org::types::type_registry& types() const {
        return types_;
    }

private:
    files bytes_;
    org::types::type_registry types_;
    std::deque<org::types::tables> tables_;
    ct::checkout_files files_;
    ct::corpus corpus_;
};

ct::configuration settings() {
    ct::configuration c;
    c.set("codegen", ct::enabled_key, "true");
    c.set("codegen", "library", "lib/");
    c.set("codegen", "convention_component", "a");
    c.set("codegen", "year", "2031");
    return c;
}

struct outcome {
    ct::run_result run;
    std::string text;
};

outcome run(const files& all, const ct::dependency_record& previous = {}) {
    const checkout c(all);
    const std::array codecs{codegen::codecs::codegen};
    const auto r = codec::ops::make_registry(codecs, c.types(), settings());
    REQUIRE(r.problems.empty());
    auto result = codec::ops::run(r, c.corpus(), c.types(), 1, previous);
    std::string text;
    if (result.files[0].size() == 1)
        text = result.files[0].contents[{0}];
    return {std::move(result), text};
}

/**
 * @brief A directory with an executable named fake-format in bin/.
 */
class scoped_bin final {
public:
    scoped_bin() {
        root_ = std::filesystem::temp_directory_path() /
            ("compass.codegen.bin." + std::to_string(::getpid()));
        std::filesystem::create_directories(root_ / "bin");
        const auto program = root_ / "bin/fake-format";
        std::ofstream(program) << "#!/bin/sh\n";
        std::filesystem::permissions(program,
            std::filesystem::perms::owner_all);
    }
    ~scoped_bin() { std::filesystem::remove_all(root_); }
    scoped_bin(const scoped_bin&) = delete;
    scoped_bin& operator=(const scoped_bin&) = delete;

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

ct::option_values values_of(const std::array<std::string, 7>& values) {
    return {codegen::codecs::codegen.options, values};
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
    CHECK(unrelated.run.skipped == counts{1});

    auto model = library();
    model["projects/ores.a/modeling/component_overview.org"] =
        "#+title: ores.a\n#+type: ores.codegen.component\n#+name: a\n"
        "#+full_name: ores.b\n#+brief: A\n#+component_kind: flat\n";
    const auto remodelled = run(model, first.run.record);
    CHECK(remodelled.run.ran == counts{1});
    CHECK(remodelled.text.find("project(ores.b)") != std::string::npos);

    auto edited = library();
    edited["lib/templates/root.mustache"] = "name({{component.name}})\n";
    const auto retemplated = run(edited, first.run.record);
    CHECK(retemplated.run.ran == counts{1});
    CHECK(retemplated.text.find("name(a)") != std::string::npos);
}

TEST_CASE("the codegen codec runs again when a model appears in a "
    "modeling directory", "[codegen][codec]") {
    const auto first = run(library());
    auto added = library();
    added["projects/ores.a/modeling/notes.org"] = "#+title: More\n";

    CHECK(run(added, first.run.record).run.ran == counts{1});
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
}

TEST_CASE("the codegen codec needs its library and convention component",
    "[codegen][codec]") {
    CHECK_THROWS_AS(codegen::codecs::settings_of(values_of(
        {"", "refdata", "", "", "", "", ""})), std::runtime_error);
    CHECK_THROWS_AS(codegen::codecs::settings_of(values_of(
        {"lib", "", "", "", "", "", ""})), std::runtime_error);
    CHECK_THROWS_AS(codegen::codecs::settings_of(values_of(
        {"lib", "refdata", "20x6", "", "", "", ""})), std::runtime_error);
    const auto s = codegen::codecs::settings_of(values_of(
        {"lib//", "refdata", "2031", "", "", "", ""}));
    CHECK(s.library == "lib");
    CHECK(s.year == 2031);
}

TEST_CASE("the formatters are ores.codegen's, with the programs and styles "
    "the options name", "[codegen][codec]") {
    const scoped_bin bin;
    CHECK(codegen::codecs::formatters_of(values_of(
        {"lib", "a", "", "", "", "", ""}), bin.root()).empty());

    const auto both = codegen::codecs::formatters_of(values_of(
        {"lib", "a", "", "missing/clang-format,bin/fake-format",
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
        {"lib", "a", "", "missing/clang-format", ".clang-format", "", ""}),
        bin.root()), std::runtime_error);
    CHECK_THROWS_AS(codegen::codecs::formatters_of(values_of(
        {"lib", "a", "", "bin/fake-format", "", "", ""}), bin.root()),
        std::runtime_error);
}

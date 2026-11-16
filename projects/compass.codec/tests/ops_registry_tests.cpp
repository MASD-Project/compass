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
#include "compass.codec/ops/registry.hpp"

#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.org/ops/type_lookup.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;
namespace ot = compass::org::types;
using Catch::Matchers::ContainsSubstring;

void no_files(const types::corpus_view&, types::file_table&) {}
void no_findings(const types::document&, const types::corpus_view&,
    types::diagnostic_table&) {}

constexpr std::array<std::string_view, 1> task_only{"task"};
constexpr std::array<std::string_view, 2> task_and_unknown{"task", "epic"};
constexpr std::array<types::option, 2> two_options{{
    {"path", "out.tsv", ""}, {"depth", "3", ""}}};

types::descriptor files_codec(std::string_view name, types::inputs reads) {
    return {.name = name, .version = 1, .summary = "", .reads = reads,
        .options = two_options, .rules = {}, .enabled_by_default = true,
        .run = types::corpus_function<types::file_table>{no_files}};
}

std::vector<std::string> values(const types::registry& r, types::codec_id c) {
    const auto v = ops::option_values(r, c);
    return {v.begin(), v.end()};
}

}

TEST_CASE("a listed input type resolves to its row of the type registry",
    "[ops][registry]") {
    const tests::test_corpus t;
    const std::array codecs{
        files_codec("tasks", types::inputs::of_types(task_only))};
    const auto r = ops::make_registry(codecs, t.types(), {});

    const types::codec_id c{0};
    CHECK(r.problems.empty());
    CHECK(ops::reads(r, c, compass::org::ops::find_type(t.types(), "task")));
    CHECK_FALSE(
        ops::reads(r, c, compass::org::ops::find_type(t.types(), "story")));
    CHECK_FALSE(ops::reads(r, c, ot::type_id::none()));
}

TEST_CASE("an input type the registry does not hold is a problem and the "
    "other types still resolve", "[ops][registry]") {
    const tests::test_corpus t;
    const std::array codecs{
        files_codec("tasks", types::inputs::of_types(task_and_unknown))};
    const auto r = ops::make_registry(codecs, t.types(), {});

    REQUIRE(r.problems.size() == 1);
    CHECK_THAT(r.problems[0], ContainsSubstring("'tasks'") &&
        ContainsSubstring("'epic'"));
    CHECK(ops::reads(r, {0}, compass::org::ops::find_type(t.types(), "task")));
}

TEST_CASE("the typed scope reads every registered type and the all scope "
    "also reads documents with none", "[ops][registry]") {
    const tests::test_corpus t;
    const std::array codecs{
        files_codec("typed", types::inputs::every_typed_document()),
        files_codec("all", types::inputs::every_document())};
    const auto r = ops::make_registry(codecs, t.types(), {});

    const auto story = compass::org::ops::find_type(t.types(), "story");
    CHECK(ops::reads(r, {0}, story));
    CHECK_FALSE(ops::reads(r, {0}, ot::type_id::none()));
    CHECK(ops::reads(r, {1}, story));
    CHECK(ops::reads(r, {1}, ot::type_id::none()));
}

TEST_CASE("the function of a descriptor says its unit and its output",
    "[types][descriptor]") {
    auto d = files_codec("c", types::inputs::every_document());
    CHECK(types::unit_of(d) == types::unit::corpus);
    CHECK(types::output_of(d) == types::output_kind::files);

    d.run = types::document_function<types::diagnostic_table>{no_findings};
    CHECK(types::unit_of(d) == types::unit::document);
    CHECK(types::output_of(d) == types::output_kind::diagnostics);
}

TEST_CASE("a codec is enabled as its descriptor says until the "
    "configuration says otherwise", "[ops][registry]") {
    const tests::test_corpus t;
    auto off = files_codec("off", types::inputs::every_document());
    off.enabled_by_default = false;
    const std::array codecs{
        files_codec("on", types::inputs::every_document()), off};

    const auto defaults = ops::make_registry(codecs, t.types(), {});
    CHECK(defaults.enabled == std::vector<std::uint8_t>{1, 0});

    types::configuration settings;
    settings.set("on", "enabled", "false");
    settings.set("off", "enabled", "true");
    const auto swapped = ops::make_registry(codecs, t.types(), settings);
    CHECK(swapped.problems.empty());
    CHECK(swapped.enabled == std::vector<std::uint8_t>{0, 1});
}

TEST_CASE("an option has its default until the configuration sets it and "
    "the later setting wins", "[ops][registry]") {
    const tests::test_corpus t;
    const std::array codecs{
        files_codec("c", types::inputs::every_document())};

    const auto defaults = ops::make_registry(codecs, t.types(), {});
    CHECK(values(defaults, {0}) == std::vector<std::string>{"out.tsv", "3"});

    types::configuration settings;
    settings.set("c", "depth", "5");
    settings.set("c", "depth", "7");
    const auto set = ops::make_registry(codecs, t.types(), settings);
    CHECK(values(set, {0}) == std::vector<std::string>{"out.tsv", "7"});
}

TEST_CASE("a setting nobody declared is a problem and changes nothing",
    "[ops][registry]") {
    const tests::test_corpus t;
    const std::array codecs{
        files_codec("c", types::inputs::every_document())};
    types::configuration settings;
    settings.set("missing", "enabled", "false");
    settings.set("c", "colour", "red");
    settings.set("c", "enabled", "no");
    const auto r = ops::make_registry(codecs, t.types(), settings);

    REQUIRE(r.problems.size() == 3);
    CHECK_THAT(r.problems[0], ContainsSubstring("missing.enabled"));
    CHECK_THAT(r.problems[1], ContainsSubstring("c.colour"));
    CHECK_THAT(r.problems[2], ContainsSubstring("c.enabled"));
    CHECK(r.enabled == std::vector<std::uint8_t>{1});
    CHECK(values(r, {0}) == std::vector<std::string>{"out.tsv", "3"});
}

TEST_CASE("a codec name used twice keeps the first codec",
    "[ops][registry]") {
    const tests::test_corpus t;
    const std::array codecs{
        files_codec("c", types::inputs::every_document()),
        files_codec("c", types::inputs::every_typed_document())};
    const auto r = ops::make_registry(codecs, t.types(), {});

    CHECK(r.size() == 1);
    REQUIRE(r.problems.size() == 1);
    CHECK_THAT(r.problems[0], ContainsSubstring("'c'"));
    CHECK(ops::reads(r, {0}, ot::type_id::none()));
}

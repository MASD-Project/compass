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
#include "compass.codec/codecs/evidence_location.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/codecs/document_index.hpp"
#include "compass.codec/codecs/required_sections.hpp"
#include "compass.codec/ops/dependencies.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;
namespace ot = compass::org::types;

constexpr std::string_view incomplete_task =
    ":PROPERTIES:\n:ID: T-1\n:END:\n#+type: task\n"
    "#+filetags: :probe:evidence:\n* Goal\n";

types::run_result run_one(const types::descriptor& d,
    const tests::test_corpus& t) {
    const std::array codecs{d};
    const auto r = ops::make_registry(codecs, t.types(), {});
    return ops::run(r, t.corpus(), t.types(), 2);
}

/**
 * @brief Writes the path of each document it can see, then what looking
 * up the path and the ID T-1 finds.
 */
void list_visible(const types::corpus_view& v, types::file_output& out) {
    std::string text;
    for (std::size_t i = 0; i < v.size(); ++i)
        text += std::string(v[i].path) + "\n";
    text += v.find_path("evidence/task.org") ? "path found\n" : "no path\n";
    const auto id = v.find_id("T-1");
    text += id ? "T-1 is " + std::string(id->path) + "\n" : "no T-1\n";
    out.files.add("visible.txt", text);
}

constexpr types::descriptor live_reader{.name = "live_reader",
    .version = 1, .summary = "", .reads = types::inputs::every_document(),
    .options = {}, .rules = {}, .enabled_by_default = true,
    .run = types::corpus_function<types::file_output>{list_visible}};

constexpr types::descriptor evidence_reader{.name = "evidence_reader",
    .version = 1, .summary = "",
    .reads = types::inputs::every_document().with_evidence(),
    .options = {}, .rules = {}, .enabled_by_default = true,
    .run = types::corpus_function<types::file_output>{list_visible}};

std::string visible_to(const types::descriptor& d,
    const tests::test_corpus& t) {
    const auto result = run_one(d, t);
    REQUIRE(result.files[0].size() == 1);
    return std::string(result.files[0].contents[{0}]);
}

}

TEST_CASE("an evidence document is not held to the contract of its type",
    "[codecs][evidence]") {
    tests::test_corpus t;
    t.add("evidence/task.org", incomplete_task);
    t.add("live/task.org", "#+type: task\n* Goal\n");

    const auto d = run_one(codecs::required_sections, t).diagnostics[0];

    REQUIRE(d.size() == 1);
    CHECK(d.documents[{0}] == ot::document_id{1});
    CHECK(d.messages[{0}] == "task document has no \"Result\" section");
}

TEST_CASE("a codec that does not declare evidence neither sees nor finds "
    "an evidence document", "[codecs][evidence]") {
    tests::test_corpus t;
    t.add("evidence/task.org", incomplete_task);
    t.add("live/story.org", "#+type: story\n");

    CHECK(visible_to(live_reader, t) ==
        "live/story.org\nno path\nno T-1\n");
    CHECK(visible_to(evidence_reader, t) ==
        "evidence/task.org\nlive/story.org\npath found\n"
        "T-1 is evidence/task.org\n");
}

TEST_CASE("a live document holds its ID against an evidence copy, "
    "wherever the copy sits in the corpus", "[codecs][evidence]") {
    tests::test_corpus t;
    t.add("a/evidence/task.org", incomplete_task);
    t.add("b/task.org", ":PROPERTIES:\n:ID: T-1\n:END:\n#+type: task\n");

    const auto index = ops::index_corpus(t.corpus());

    CHECK(index.evidence == std::vector<std::uint8_t>{1, 0});
    CHECK(index.ids.at("T-1") == ot::document_id{1});
    CHECK(visible_to(evidence_reader, t).ends_with("T-1 is b/task.org\n"));
}

TEST_CASE("the document index lists evidence with its type",
    "[codecs][evidence]") {
    tests::test_corpus t;
    t.add("evidence/task.org", incomplete_task);

    const auto result = run_one(codecs::document_index, t);

    REQUIRE(result.files[0].size() == 1);
    CHECK(std::string(result.files[0].contents[{0}]) ==
        "id\ttitle\ttype\tpath\nT-1\t\ttask\tevidence/task.org\n");
}

TEST_CASE("an evidence document outside an evidence/ directory is "
    "reported on its tags", "[codecs][evidence]") {
    tests::test_corpus t;
    t.add("doc/evidence/task.org", incomplete_task);
    t.add("evidence/top.org", "#+filetags: :evidence:\n");
    t.add("doc/live.org", "#+filetags: :knowledge:\n");
    t.add("doc/evidence_of_nothing/stray.org", std::string(incomplete_task));

    const auto d = run_one(codecs::evidence_location, t).diagnostics[0];

    REQUIRE(d.size() == 1);
    CHECK(d.documents[{0}] == ot::document_id{3});
    CHECK(d.ranges[{0}].of(incomplete_task) == ":probe:evidence:");
    CHECK(codecs::evidence_location.rules[d.rules[{0}].value].name ==
        "outside_evidence_directory");
}

TEST_CASE("a tag that only contains evidence does not make evidence",
    "[codecs][evidence]") {
    tests::test_corpus t;
    t.add("doc/task.org", "#+type: task\n#+filetags: :evidenced:\n* Goal\n");

    CHECK(run_one(codecs::required_sections, t).diagnostics[0].size() == 1);
    CHECK(ops::index_corpus(t.corpus()).evidence ==
        std::vector<std::uint8_t>{0});
}

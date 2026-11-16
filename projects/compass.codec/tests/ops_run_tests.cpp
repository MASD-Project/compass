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
#include "compass.codec/ops/run.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.org/ops/type_lookup.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;
namespace ot = compass::org::types;

constexpr std::array<std::string_view, 1> task_only{"task"};
constexpr std::array<types::option, 1> prefix_option{{
    {"prefix", "seen/", ""}}};
constexpr std::array<types::rule, 1> probe_rules{{{"seen", ""}}};

/**
 * @brief Emits one empty file per visible document, named by the prefix
 * option and the path of the document.
 */
void list_corpus(const types::corpus_view& corpus, types::file_table& out) {
    for (std::size_t i = 0; i < corpus.size(); ++i) {
        out.add(std::string(corpus.option("prefix")) +
            std::string(corpus[i].path), "");
    }
}

/**
 * @brief Emits one finding per call, whose message is the path of the
 * document it was called with.
 */
void list_document(const types::document& d, const types::corpus_view&,
    types::diagnostic_table& out) {
    out.add({d.row, {}, {0}, types::severity::warning, d.path, ""});
}

void fail(const types::corpus_view&, types::file_table&) {
    throw std::runtime_error("the codec was called");
}

types::descriptor corpus_probe(types::inputs reads) {
    return {.name = "corpus_probe", .version = 1, .summary = "",
        .reads = reads, .options = prefix_option, .rules = {},
        .enabled_by_default = true,
        .run = types::corpus_function<types::file_table>{list_corpus}};
}

types::descriptor document_probe(types::inputs reads) {
    return {.name = "document_probe", .version = 1, .summary = "",
        .reads = reads, .options = {}, .rules = probe_rules,
        .enabled_by_default = true,
        .run = types::document_function<types::diagnostic_table>{
            list_document}};
}

types::descriptor failing() {
    return {.name = "failing", .version = 1, .summary = "",
        .reads = types::inputs::every_document(), .options = {},
        .rules = {}, .enabled_by_default = true,
        .run = types::corpus_function<types::file_table>{fail}};
}

template <typename Handle, typename Offset>
std::vector<std::string> texts(
    const types::text_column<Handle, Offset>& column) {
    std::vector<std::string> out;
    for (std::uint32_t i = 0; i < column.size(); ++i)
        out.emplace_back(column[Handle{i}]);
    return out;
}

using strings = std::vector<std::string>;

/**
 * @brief A story, two tasks, a document of a type nobody registered and
 * one with no type.
 */
void fill(tests::test_corpus& t) {
    t.add("story.org", "#+type: story\n* Goal\n* Status\n");
    t.add("task_a.org", "#+type: task\n* Goal\n* Result\n");
    t.add("notes.org", "#+type: scribble\n");
    t.add("task_b.org", "#+type: task\n* Goal\n");
    t.add("readme.org", "Plain text.\n");
}

}

TEST_CASE("the type of a document is the registered type its first "
    "#+type: names", "[ops][run]") {
    tests::test_corpus t;
    fill(t);
    const auto& c = t.corpus();
    CHECK(c.types[0] == compass::org::ops::find_type(t.types(), "story"));
    CHECK(c.types[1] == compass::org::ops::find_type(t.types(), "task"));
    CHECK(c.types[2].is_none());
    CHECK(c.types[4].is_none());
}

TEST_CASE("a corpus codec sees the documents of its declared types and no "
    "others", "[ops][run]") {
    tests::test_corpus t;
    fill(t);
    const std::array codecs{
        corpus_probe(types::inputs::of_types(task_only))};
    const auto r = ops::make_registry(codecs, t.types(), {});

    const auto result = ops::run(r, t.corpus(), t.types(), 2);

    CHECK(result.documents == std::vector<std::uint64_t>{2});
    CHECK(texts(result.files[0].paths) ==
        strings{"seen/task_a.org", "seen/task_b.org"});
}

TEST_CASE("a document codec is called once per document of its declared "
    "types and for no other", "[ops][run]") {
    tests::test_corpus t;
    fill(t);
    const std::array codecs{
        document_probe(types::inputs::of_types(task_only))};
    const auto r = ops::make_registry(codecs, t.types(), {});

    const auto result = ops::run(r, t.corpus(), t.types(), 2);

    CHECK(texts(result.diagnostics[0].messages) ==
        strings{"task_a.org", "task_b.org"});
    CHECK(result.diagnostics[0].documents.values ==
        std::vector<ot::document_id>{{1}, {3}});
}

TEST_CASE("the typed scope hides documents with no registered type and the "
    "all scope shows them", "[ops][run]") {
    tests::test_corpus t;
    fill(t);
    const std::array typed{
        corpus_probe(types::inputs::every_typed_document())};
    const std::array all{corpus_probe(types::inputs::every_document())};

    const auto typed_result = ops::run(
        ops::make_registry(typed, t.types(), {}), t.corpus(), t.types(), 1);
    const auto all_result = ops::run(
        ops::make_registry(all, t.types(), {}), t.corpus(), t.types(), 1);

    CHECK(texts(typed_result.files[0].paths) == strings{"seen/story.org",
        "seen/task_a.org", "seen/task_b.org"});
    CHECK(all_result.files[0].size() == 5);
}

TEST_CASE("a disabled codec is not called and an enabled one beside it is",
    "[ops][run]") {
    tests::test_corpus t;
    fill(t);
    const std::array codecs{failing(),
        corpus_probe(types::inputs::every_document())};
    types::configuration settings;
    settings.set("failing", "enabled", "false");
    const auto r = ops::make_registry(codecs, t.types(), settings);

    const auto result = ops::run(r, t.corpus(), t.types(), 2);

    CHECK(result.documents == std::vector<std::uint64_t>{0, 5});
    CHECK(result.files[0].size() == 0);
    CHECK(result.files[1].size() == 5);
}

TEST_CASE("what an enabled codec throws reaches the caller", "[ops][run]") {
    tests::test_corpus t;
    fill(t);
    const std::array codecs{failing(),
        corpus_probe(types::inputs::every_document())};
    const auto r = ops::make_registry(codecs, t.types(), {});

    CHECK_THROWS_AS(ops::run(r, t.corpus(), t.types(), 2),
        std::runtime_error);
}

TEST_CASE("a codec reads the configured value of its option", "[ops][run]") {
    tests::test_corpus t;
    t.add("a.org", "#+type: task\n");
    const std::array codecs{corpus_probe(types::inputs::every_document())};
    types::configuration settings;
    settings.set("corpus_probe", "prefix", "out/");
    const auto r = ops::make_registry(codecs, t.types(), settings);

    const auto result = ops::run(r, t.corpus(), t.types(), 1);

    CHECK(texts(result.files[0].paths) == strings{"out/a.org"});
}

TEST_CASE("an option the descriptor does not declare throws", "[ops][run]") {
    tests::test_corpus t;
    const types::corpus_view view(t.corpus(), t.types(), {}, {}, {});
    CHECK_THROWS_AS(view.option("prefix"), std::out_of_range);
}

TEST_CASE("the rows of a document codec are in document order on any "
    "thread count", "[ops][run]") {
    tests::test_corpus t;
    strings expected;
    for (int i = 0; i < 100; ++i) {
        const auto path = "task_" + std::to_string(1000 + i) + ".org";
        t.add(path, "#+type: task\n");
        expected.push_back(path);
    }
    const std::array codecs{
        document_probe(types::inputs::of_types(task_only))};
    const auto r = ops::make_registry(codecs, t.types(), {});

    for (const unsigned threads : {1u, 3u, 8u}) {
        const auto result = ops::run(r, t.corpus(), t.types(), threads);
        CHECK(texts(result.diagnostics[0].messages) == expected);
    }
}

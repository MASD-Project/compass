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
#include "compass.codec/ops/dependencies.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/codecs/document_index.hpp"
#include "compass.codec/codecs/required_sections.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;
namespace ot = compass::org::types;

using sources = std::map<std::string, std::string>;
using counts = std::vector<std::uint64_t>;

constexpr std::array<std::string_view, 2> stories_and_tasks{"story", "task"};
constexpr std::array<std::string_view, 1> tasks{"task"};
constexpr std::array<types::rule, 1> link_rules{{{"link", ""}}};

/**
 * @brief Reports where each id: and file: link of a document leads, found
 * through the view, or that it leads nowhere.
 */
void follow_links(const types::document& d, const types::corpus_view& corpus,
    types::diagnostic_table& out) {
    const auto& links = d.tables.links;
    for (std::uint32_t i = 0; i < links.size(); ++i) {
        const ot::link_id l{i};
        const auto scheme = compass::org::ops::name_text(d.tables,
            links.types[l]);
        if (scheme != "id" && scheme != "file")
            continue;
        const auto target = links.paths[l].of(d.source);
        const auto found = scheme == "id" ? corpus.find_id(target)
            : corpus.find_path(target);
        const auto message = found
            ? "links to " + std::string(found->path)
            : "broken link to " + std::string(target);
        out.add({d.row, {}, {0}, types::severity::warning, message, ""});
    }
}

/**
 * @brief Emits one file listing the path of every task.
 */
void list_tasks(const types::corpus_view& corpus, types::file_output& out) {
    std::string text;
    for (std::size_t i = 0; i < corpus.size(); ++i)
        text += std::string(corpus[i].path) + "\n";
    out.files.add("tasks.txt", text);
}

types::descriptor link_follower(std::uint32_t version = 1,
    std::string_view code = "") {
    return {.name = "follow_links", .version = version, .code = code,
        .summary = "",
        .reads = types::inputs::of_types(stories_and_tasks), .options = {},
        .rules = link_rules, .enabled_by_default = true,
        .run = types::document_function<types::diagnostic_table>{
            follow_links}};
}

types::descriptor task_list() {
    return {.name = "task_list", .version = 1, .summary = "",
        .reads = types::inputs::of_types(tasks), .options = {},
        .rules = {}, .enabled_by_default = true,
        .run = types::corpus_function<types::file_output>{list_tasks}};
}

/**
 * @brief document_index, required_sections, follow_links and task_list,
 * in that order.
 */
std::array<types::descriptor, 4> all_codecs(std::uint32_t links_version = 1,
    std::string_view links_code = "") {
    return {codecs::document_index, codecs::required_sections,
        link_follower(links_version, links_code), task_list()};
}

/**
 * @brief A story with an ID, a task linking to it and to an ID nobody
 * has, a task lacking a section that links to a missing file, a third
 * task and a document with no type.
 */
sources base() {
    return {
        {"story.org", ":PROPERTIES:\n:ID: S-1\n:END:\n#+type: story\n"
                      "* Goal\n* Status\n"},
        {"task_a.org", "#+type: task\n* Goal\nSee [[id:S-1][the story]] "
                       "and [[id:X-404][later]].\n* Result\n"},
        {"task_b.org", "#+type: task\n* Goal\nSee [[file:extra.org]].\n"},
        {"task_c.org", "#+type: task\n* Goal\n* Result\n"},
        {"readme.org", "Plain text.\n"}};
}

/**
 * @brief The outputs of a run without corpus rows: per codec, each file as
 * path and content and each finding as document path and message.
 */
std::vector<std::vector<std::string>> outputs(const types::run_result& r,
    const types::corpus& c) {
    std::vector<std::vector<std::string>> out(r.files.size());
    for (std::size_t codec = 0; codec < r.files.size(); ++codec) {
        const auto& f = r.files[codec];
        for (std::uint32_t i = 0; i < f.size(); ++i)
            out[codec].push_back(std::string(f.paths[{i}]) + "=" +
                std::string(f.contents[{i}]));
        const auto& d = r.diagnostics[codec];
        for (std::uint32_t i = 0; i < d.size(); ++i)
            out[codec].push_back(
                std::string(c.paths[d.documents[{i}].value]) + ": " +
                std::string(d.messages[{i}]));
    }
    return out;
}

/**
 * @brief Runs @p codecs over @p docs with the record of an earlier run,
 * and checks that the outputs are those of a run with no record.
 */
types::run_result run(const sources& docs,
    const types::dependency_record& previous,
    std::span<const types::descriptor> codecs,
    const types::configuration& settings = {}) {
    tests::test_corpus t;
    for (const auto& [path, source] : docs)
        t.add(path, source);
    const auto r = ops::make_registry(codecs, t.types(), settings);
    auto result = ops::run(r, t.corpus(), t.types(), 2, previous);
    const auto fresh = ops::run(r, t.corpus(), t.types(), 2);
    CHECK(outputs(result, t.corpus()) == outputs(fresh, t.corpus()));
    return result;
}

types::run_result run(const sources& docs,
    const types::dependency_record& previous = {}) {
    const auto codecs = all_codecs();
    return run(docs, previous, codecs);
}

std::vector<std::string> messages(const types::diagnostic_table& t) {
    std::vector<std::string> out;
    for (std::uint32_t i = 0; i < t.size(); ++i)
        out.emplace_back(t.messages[{i}]);
    return out;
}

}

TEST_CASE("a first run calls every unit and a second run with nothing "
    "changed calls none", "[ops][dependencies]") {
    const auto first = run(base());
    CHECK(first.ran == counts{1, 4, 4, 1});
    CHECK(first.skipped == counts{0, 0, 0, 0});

    const auto second = run(base(), first.record);

    CHECK(second.ran == counts{0, 0, 0, 0});
    CHECK(second.skipped == counts{1, 4, 4, 1});
    CHECK(ops::encode_dependency_record(second.record) ==
        ops::encode_dependency_record(first.record));
}

TEST_CASE("editing a document calls its document units and every corpus "
    "unit that may read it", "[ops][dependencies]") {
    const auto first = run(base());
    auto docs = base();
    docs["task_c.org"] = "#+type: task\n* Goal\n";

    const auto second = run(docs, first.record);

    CHECK(second.ran == counts{1, 1, 1, 1});
    REQUIRE(second.diagnostics[1].size() == 2);
}

TEST_CASE("editing a document of a type a corpus codec does not read "
    "leaves that codec alone", "[ops][dependencies]") {
    const auto first = run(base());
    auto docs = base();
    docs["readme.org"] = "Other text.\n";

    const auto second = run(docs, first.record);

    CHECK(second.ran == counts{1, 0, 0, 0});
}

TEST_CASE("a unit that found a document by its ID runs again when that "
    "document changes and not when an unrelated one does",
    "[ops][dependencies]") {
    const auto first = run(base());

    auto edited_story = base();
    edited_story["story.org"] += "Some words.\n";
    const auto story = run(edited_story, first.record);
    CHECK(story.ran[2] == 2);

    auto edited_task = base();
    edited_task["task_c.org"] += "Some words.\n";
    const auto unrelated = run(edited_task, first.record);
    CHECK(unrelated.ran[2] == 1);
}

TEST_CASE("a unit whose ID lookup found nothing runs again when a document "
    "with that ID appears", "[ops][dependencies]") {
    const auto first = run(base());
    CHECK(messages(first.diagnostics[2]) == std::vector<std::string>{
        "links to story.org", "broken link to X-404",
        "broken link to extra.org"});

    auto docs = base();
    docs["later.org"] = ":PROPERTIES:\n:ID: X-404\n:END:\n#+type: task\n"
                        "* Goal\n* Result\n";
    const auto second = run(docs, first.record);

    CHECK(second.ran[2] == 2);
    CHECK(messages(second.diagnostics[2]) == std::vector<std::string>{
        "links to story.org", "links to later.org",
        "broken link to extra.org"});
}

TEST_CASE("a document with the looked-up ID that the codec may not read "
    "leaves the lookup a miss", "[ops][dependencies]") {
    const auto first = run(base());
    auto docs = base();
    docs["later.org"] = ":PROPERTIES:\n:ID: X-404\n:END:\nPlain.\n";

    const auto second = run(docs, first.record);

    CHECK(second.ran[2] == 0);
}

TEST_CASE("a unit whose path lookup found nothing runs again when a "
    "document appears at that path", "[ops][dependencies]") {
    const auto first = run(base());
    auto docs = base();
    docs["extra.org"] = "#+type: task\n* Goal\n* Result\n";

    const auto second = run(docs, first.record);

    CHECK(second.ran[2] == 2);
    CHECK(messages(second.diagnostics[2]).back() == "links to extra.org");
}

TEST_CASE("adding or removing a document of a declared type calls the "
    "corpus unit again", "[ops][dependencies]") {
    const auto first = run(base());

    auto added = base();
    added["task_d.org"] = "#+type: task\n* Goal\n* Result\n";
    const auto after_add = run(added, first.record);
    CHECK(after_add.ran[3] == 1);
    REQUIRE(after_add.files[3].size() == 1);
    CHECK(after_add.files[3].contents[{0}] ==
        "task_a.org\ntask_b.org\ntask_c.org\ntask_d.org\n");

    auto removed = base();
    removed.erase("task_c.org");
    const auto after_remove = run(removed, first.record);
    CHECK(after_remove.ran[3] == 1);
    CHECK(after_remove.ran[1] == 0);

    auto untyped = base();
    untyped["notes.org"] = "Notes.\n";
    const auto after_untyped = run(untyped, first.record);
    CHECK(after_untyped.ran == counts{1, 0, 0, 0});
}

TEST_CASE("bumping the version of a codec calls only that codec's units",
    "[ops][dependencies]") {
    const auto first = run(base());
    const auto bumped = all_codecs(2);

    const auto second = run(base(), first.record, bumped);

    CHECK(second.ran == counts{0, 0, 4, 0});
}

TEST_CASE("a build that changes the code of a codec calls only that "
    "codec's units, and one with the same code calls none",
    "[ops][dependencies]") {
    const auto built = all_codecs(1, "code-a");
    const auto first = run(base(), {}, built);
    const auto rebuilt = all_codecs(1, "code-b");

    const auto second = run(base(), first.record, rebuilt);
    const auto third = run(base(), second.record, rebuilt);

    CHECK(second.ran == counts{0, 0, 4, 0});
    CHECK(third.ran == counts{0, 0, 0, 0});
}

TEST_CASE("changing the configuration of a codec calls only that codec's "
    "units", "[ops][dependencies]") {
    const auto first = run(base());
    const auto codecs = all_codecs();
    types::configuration settings;
    settings.set("document_index", "path", "index.tsv");

    const auto second = run(base(), first.record, codecs, settings);

    CHECK(second.ran == counts{1, 0, 0, 0});
    REQUIRE(second.files[0].size() == 1);
    CHECK(second.files[0].paths[{0}] == "index.tsv");
}

namespace {

std::string outside_the_checkout;

std::array<types::descriptor, 4> codecs_with_environment() {
    auto codecs = all_codecs();
    codecs[3].environment = [](const types::option_values&) {
        return outside_the_checkout;
    };
    return codecs;
}

}

TEST_CASE("a change to what a codec reads outside the checkout calls only "
    "that codec's units", "[ops][dependencies]") {
    const auto codecs = codecs_with_environment();
    outside_the_checkout = "a library";
    const auto first = run(base(), {}, codecs);
    const auto same = run(base(), first.record, codecs);
    outside_the_checkout = "another library";
    const auto changed = run(base(), same.record, codecs);

    CHECK(same.ran == counts{0, 0, 0, 0});
    CHECK(changed.ran == counts{0, 0, 0, 1});
}

TEST_CASE("a unit that read the type registry runs again when the "
    "registry changes", "[ops][dependencies]") {
    const auto first = run(base());
    const auto codecs = all_codecs();
    tests::test_corpus t({tests::story_contract,
        R"(#+title: Document type: task

* Contract

- Required sections ::
  - =* Goal= — one paragraph.
)"});
    for (const auto& [path, source] : base())
        t.add(path, source);
    const auto r = ops::make_registry(codecs, t.types(), {});

    const auto second = ops::run(r, t.corpus(), t.types(), 2, first.record);

    CHECK(second.ran == counts{0, 4, 0, 0});
    CHECK(second.diagnostics[1].size() == 0);
}

TEST_CASE("the record survives encoding and a damaged one is rejected",
    "[ops][dependencies]") {
    const auto first = run(base());
    const auto bytes = ops::encode_dependency_record(first.record);
    CHECK(ops::encode_dependency_record(
        ops::decode_dependency_record(bytes)) == bytes);

    auto damaged = bytes;
    damaged[damaged.size() / 2] ^= 0x01;
    CHECK_THROWS_AS(ops::decode_dependency_record(damaged),
        std::runtime_error);
    CHECK_THROWS_AS(ops::decode_dependency_record(bytes.substr(0, 30)),
        std::runtime_error);
}

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
#include "compass.codec/ops/action_cache.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/codecs/document_index.hpp"
#include "compass.codec/io/action_cache.hpp"
#include "compass.codec/ops/dependencies.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/io/pack_store.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;
namespace ot = compass::org::types;

using sources = std::map<std::string, std::string>;
using counts = std::vector<std::uint64_t>;

constexpr std::array<std::string_view, 2> stories_and_tasks{"story", "task"};
constexpr std::array<types::rule, 1> quote_rules{{{"quote", ""}}};
constexpr std::array<types::option, 1> quote_options{{
    {"prefix", "quotes", ""}}};

/**
 * @brief Reports, for each id: link of a document, the source of the
 * document it leads to, or that it leads nowhere, so a finding changes
 * with the content of the document looked up.
 */
void quote_links(const types::document& d, const types::corpus_view& corpus,
    types::diagnostic_table& out) {
    const auto& links = d.tables.links;
    for (std::uint32_t i = 0; i < links.size(); ++i) {
        const ot::link_id l{i};
        if (compass::org::ops::name_text(d.tables, links.types[l]) != "id")
            continue;
        const auto found = corpus.find_id(links.paths[l].of(d.source));
        const auto message = std::string(corpus.option("prefix")) + " " +
            (found ? std::string(found->source) : std::string("nothing"));
        out.add({d.row, {}, {0}, types::severity::warning, message, ""});
    }
}

types::descriptor link_quoter(std::uint32_t version = 1,
    std::string_view code = "") {
    return {.name = "quote_links", .version = version, .code = code,
        .summary = "",
        .reads = types::inputs::of_types(stories_and_tasks),
        .options = quote_options, .rules = quote_rules,
        .enabled_by_default = true,
        .run = types::document_function<types::diagnostic_table>{
            quote_links}};
}

/**
 * @brief A story with an ID, a task quoting it and an ID nobody has, and
 * two tasks that link nowhere.
 */
sources base() {
    return {
        {"story.org", ":PROPERTIES:\n:ID: S-1\n:END:\n#+type: story\n"
                      "* Goal\n* Status\n"},
        {"task_a.org", "#+type: task\n* Goal\nSee [[id:S-1][the story]] "
                       "and [[id:X-404][later]].\n* Result\n"},
        {"task_b.org", "#+type: task\n* Goal\n"},
        {"task_c.org", "#+type: task\n* Goal\n* Result\n"}};
}

/**
 * @brief The git common directory both worktrees share, as far as the
 * cache is concerned.
 */
class scoped_cache final {
    compass::testing::filesystem::scoped_directory dir_{
        "compass.codec.cache_tests"};

public:
    [[nodiscard]] const std::filesystem::path& root() const {
        return dir_.path();
    }

    /**
     * @brief Where each stored action object of @p kind is: its pack and
     * the offset of its bytes.
     */
    [[nodiscard]] std::vector<std::pair<std::filesystem::path,
        std::uint64_t>> entries(compass::store::types::action_kind kind) const {
        namespace store = compass::store;
        std::vector<std::pair<std::filesystem::path, std::uint64_t>> out;
        for (const auto& p :
            store::io::open_packs(store::io::packs_dir(dir_.path()))) {
            for (std::uint64_t row = 0; row < p.view.count; ++row) {
                const auto e = store::ops::entry_at(p.view, row);
                if (e.kind == store::types::entry_kind_of(kind))
                    out.emplace_back(p.path, e.offset);
            }
        }
        return out;
    }
};

/**
 * @brief What one worktree's run produced, in bytes that hold no corpus
 * row: its record, which holds every output, and its counts.
 */
struct outcome {
    counts ran;
    counts skipped;
    counts cached;
    std::string record;
};

/**
 * @brief Runs document_index and quote_links over @p docs as a worktree
 * with the record @p previous would, with the cache of @p cache, stores
 * what ran, and checks the outputs equal those of a run with neither.
 */
outcome run(const scoped_cache& cache, const sources& docs,
    const types::dependency_record& previous = {},
    const types::configuration& settings = {}, std::uint32_t version = 1,
    types::dependency_record* record = nullptr, std::string_view code = "") {
    tests::test_corpus t;
    for (const auto& [path, source] : docs)
        t.add(path, source);
    const std::array<types::descriptor, 2> codecs{codecs::document_index,
        link_quoter(version, code)};
    const auto r = ops::make_registry(codecs, t.types(), settings);
    const auto result = ops::run(r, t.corpus(), t.types(), 2, previous,
        io::action_source_of(cache.root()));
    (void)io::store_actions(cache.root(), result.record, result.fresh, 2);

    const auto plain = ops::run(r, t.corpus(), t.types(), 2);
    for (std::size_t c = 0; c < codecs.size(); ++c) {
        const auto& f = result.files[c];
        REQUIRE(f.size() == plain.files[c].size());
        for (std::uint32_t i = 0; i < f.size(); ++i) {
            CHECK(f.paths[{i}] == plain.files[c].paths[{i}]);
            CHECK(f.contents[{i}] == plain.files[c].contents[{i}]);
        }
        CHECK(ops::encode_diagnostics(t.corpus(), result.diagnostics[c], 0,
                  static_cast<std::uint32_t>(result.diagnostics[c].size())) ==
            ops::encode_diagnostics(t.corpus(), plain.diagnostics[c], 0,
                static_cast<std::uint32_t>(plain.diagnostics[c].size())));
    }
    CHECK(ops::encode_dependency_record(result.record) ==
        ops::encode_dependency_record(plain.record));
    if (record != nullptr)
        *record = result.record;
    return {result.ran, result.skipped, result.cached,
        ops::encode_dependency_record(result.record)};
}

void damage(const std::pair<std::filesystem::path, std::uint64_t>& entry) {
    std::fstream f(entry.first, std::ios::binary | std::ios::in |
        std::ios::out);
    f.seekp(static_cast<std::streamoff>(entry.second + 12));
    f.put('!');
}

types::dependency_record files_read(
    const std::vector<std::pair<std::string, std::string>>& files) {
    types::dependency_record r;
    r.input_firsts.values.push_back({0});
    for (const auto& [path, bytes] : files) {
        r.input_kinds.values.push_back(types::input_kind::file);
        r.input_keys.add(path);
        r.input_found.values.push_back(1);
        r.input_values.values.push_back(
            compass::store::ops::blob_id_of(bytes));
    }
    return r;
}

std::vector<std::pair<std::string, std::string>> numbered_files(int n) {
    std::vector<std::pair<std::string, std::string>> files;
    for (int i = 0; i < n; ++i)
        files.emplace_back("src/file_" + std::to_string(i) + ".cpp",
            std::to_string(i));
    return files;
}

std::size_t new_chunks(const std::vector<ops::input_chunk>& before,
    const std::vector<ops::input_chunk>& after) {
    return static_cast<std::size_t>(std::ranges::count_if(after,
        [&](const ops::input_chunk& c) {
            return !std::ranges::contains(before, c.key,
                &ops::input_chunk::key);
        }));
}

}

TEST_CASE("an input set's chunks hold its inputs in the order they were "
    "read", "[ops][action_cache]") {
    const auto record = files_read(numbered_files(5000));

    const auto chunks = ops::chunk_input_set(record, {0});

    REQUIRE(chunks.size() > 1);
    types::input_list inputs;
    for (const auto& c : chunks) {
        const auto before = inputs.input_count();
        REQUIRE(ops::decode_input_chunk(c.body, inputs));
        CHECK(inputs.input_count() - before <= ops::chunk_limit);
    }
    REQUIRE(inputs.input_count() == record.input_count());
    for (std::uint32_t i = 0; i < inputs.input_count(); ++i) {
        const types::input_id in{i};
        CHECK(inputs.input_keys[in] == record.input_keys[in]);
        CHECK(inputs.input_values[in] == record.input_values[in]);
    }
}

TEST_CASE("an input whose value changed changes the one chunk that holds "
    "it", "[ops][action_cache]") {
    auto files = numbered_files(5000);
    const auto before = ops::chunk_input_set(files_read(files), {0});
    files[2500].second = "edited";

    const auto after = ops::chunk_input_set(files_read(files), {0});

    CHECK(after.size() == before.size());
    CHECK(new_chunks(before, after) == 1);
}

TEST_CASE("an input added or removed changes at most the two chunks around "
    "it", "[ops][action_cache]") {
    auto files = numbered_files(5000);
    const auto before = ops::chunk_input_set(files_read(files), {0});
    auto added = files;
    added.insert(added.begin() + 2500, {"src/new_file.cpp", "new"});
    auto removed = files;
    removed.erase(removed.begin() + 2500);

    CHECK(new_chunks(before,
        ops::chunk_input_set(files_read(added), {0})) <= 2);
    CHECK(new_chunks(before,
        ops::chunk_input_set(files_read(removed), {0})) <= 2);
}

TEST_CASE("a manifest names a changed set by its chunk keys, sharing the "
    "unchanged chunks", "[ops][action_cache]") {
    auto files = numbered_files(5000);
    types::action_manifest m;
    const auto one = ops::chunk_input_set(files_read(files), {0});
    REQUIRE(ops::add_input_set(m, {}, one));
    CHECK_FALSE(ops::add_input_set(m, {}, one));
    files[10].second = "edited";
    const auto two = ops::chunk_input_set(files_read(files), {0});
    compass::store::types::blob_id other;
    other.bytes[0] = 1;

    REQUIRE(ops::add_input_set(m, other, two));

    const auto decoded = ops::decode_manifest(ops::encode_manifest(m));
    REQUIRE(decoded);
    REQUIRE(decoded->size() == 2);
    CHECK(decoded->results.values[0] == other);
    CHECK(decoded->chunk_count() == one.size() + two.size());
    CHECK(ops::encode_manifest(m).size() <
        8 + 2 * (24 + 20 * one.size()) + 1);
}


TEST_CASE("a second worktree with no record of its own runs no unit and "
    "records what the first did", "[ops][action_cache]") {
    scoped_cache cache;
    const auto first = run(cache, base());
    CHECK(first.ran == counts{1, 4});

    const auto second = run(cache, base());

    CHECK(second.ran == counts{0, 0});
    CHECK(second.cached == counts{1, 4});
    CHECK(second.record == first.record);
}

TEST_CASE("an edited document runs its own unit and the corpus unit and "
    "takes the rest from the cache", "[ops][action_cache]") {
    scoped_cache cache;
    (void)run(cache, base());
    auto docs = base();
    docs["task_c.org"] += "Some words.\n";

    const auto second = run(cache, docs);

    CHECK(second.ran == counts{1, 1});
    CHECK(second.cached == counts{0, 3});
}

TEST_CASE("a unit that looked up a document misses the cache when that "
    "document differs between the worktrees", "[ops][action_cache]") {
    scoped_cache cache;
    (void)run(cache, base());
    auto docs = base();
    docs["story.org"] += "Other words.\n";

    const auto second = run(cache, docs);

    CHECK(second.ran == counts{1, 2});
    CHECK(second.cached == counts{0, 2});
}

TEST_CASE("a unit whose lookup missed misses the cache when a document "
    "answers it in the other worktree", "[ops][action_cache]") {
    scoped_cache cache;
    (void)run(cache, base());
    auto docs = base();
    docs["later.org"] = ":PROPERTIES:\n:ID: X-404\n:END:\n#+type: task\n"
                        "* Goal\n";

    const auto second = run(cache, docs);

    CHECK(second.ran == counts{1, 2});
    CHECK(second.cached == counts{0, 3});
}

TEST_CASE("each of several input sets of one manifest gives back its own "
    "result", "[ops][action_cache]") {
    scoped_cache cache;
    auto old_story = base();
    auto new_story = base();
    new_story["story.org"] += "Newer words.\n";
    (void)run(cache, old_story);
    (void)run(cache, new_story);

    const auto old_again = run(cache, old_story);
    const auto new_again = run(cache, new_story);

    CHECK(old_again.ran == counts{0, 0});
    CHECK(new_again.ran == counts{0, 0});
    CHECK(old_again.record != new_again.record);
}

TEST_CASE("a manifest keeps only its newest input sets",
    "[ops][action_cache]") {
    scoped_cache cache;
    for (std::size_t i = 0; i <= ops::manifest_capacity; ++i) {
        auto docs = base();
        docs["story.org"] += std::to_string(i) + "\n";
        (void)run(cache, docs);
    }
    auto first = base();
    first["story.org"] += "0\n";
    auto last = base();
    last["story.org"] += std::to_string(ops::manifest_capacity) + "\n";

    CHECK(run(cache, last).ran == counts{0, 0});
    // The corpus unit of document_index has one manifest too, so the
    // oldest state fell off it as well.
    CHECK(run(cache, first).ran == counts{1, 1});
}

TEST_CASE("a change of version or configuration misses the cache",
    "[ops][action_cache]") {
    scoped_cache cache;
    (void)run(cache, base());

    const auto newer = run(cache, base(), {}, {}, 2);
    CHECK(newer.ran == counts{0, 4});

    types::configuration settings;
    settings.set("quote_links", "prefix", "cites");
    const auto configured = run(cache, base(), {}, settings);
    CHECK(configured.ran == counts{0, 4});
}

TEST_CASE("a build whose code differs misses the cache and the build "
    "that stored a result hits it", "[ops][action_cache]") {
    scoped_cache cache;
    (void)run(cache, base(), {}, {}, 1, nullptr, "code-a");

    const auto rebuilt = run(cache, base(), {}, {}, 1, nullptr, "code-b");
    const auto reverted = run(cache, base(), {}, {}, 1, nullptr, "code-a");

    CHECK(rebuilt.ran == counts{0, 4});
    CHECK(reverted.ran == counts{0, 0});
}

TEST_CASE("a damaged result or manifest is ignored and its unit runs",
    "[ops][action_cache]") {
    using compass::store::types::action_kind;
    scoped_cache cache;
    (void)run(cache, base());
    for (const auto& entry : cache.entries(action_kind::result))
        damage(entry);

    const auto results_damaged = run(cache, base());
    CHECK(results_damaged.ran == counts{1, 4});
    CHECK(run(cache, base()).ran == counts{0, 0});

    for (const auto& entry : cache.entries(action_kind::manifest))
        damage(entry);
    const auto manifests_damaged = run(cache, base());
    CHECK(manifests_damaged.ran == counts{1, 4});
    CHECK(run(cache, base()).ran == counts{0, 0});
}

TEST_CASE("units the worktree's record stands in for are skipped, not "
    "looked up", "[ops][action_cache]") {
    scoped_cache cache;
    types::dependency_record record;
    (void)run(cache, base(), {}, {}, 1, &record);

    const auto second = run(cache, base(), record);

    CHECK(second.skipped == counts{1, 4});
    CHECK(second.cached == counts{0, 0});
}

TEST_CASE("the record names the manifest and result of every unit",
    "[io][action_cache]") {
    scoped_cache cache;
    types::dependency_record record;
    (void)run(cache, base(), {}, {}, 1, &record);

    const auto manifests = cache.entries(
        compass::store::types::action_kind::manifest);
    const auto results = cache.entries(
        compass::store::types::action_kind::result);
    std::set<compass::store::types::blob_id> chunks;
    for (std::uint32_t u = 0; u < record.size(); ++u) {
        for (const auto& c : ops::chunk_input_set(record, {u}))
            chunks.insert(c.key);
    }
    CHECK(manifests.size() == 5);
    CHECK(results.size() == 5 + chunks.size());
    const compass::store::io::pack_reader store(
        compass::store::io::packs_dir(cache.root()));
    for (std::uint32_t u = 0; u < record.size(); ++u) {
        CHECK(store.read_action(compass::store::types::action_kind::result,
            record.results[{u}]));
    }
}

TEST_CASE("two documents with the same bytes at different paths keep "
    "their own results", "[ops][action_cache]") {
    scoped_cache cache;
    auto docs = base();
    docs["task_d.org"] = docs["task_a.org"];
    const auto first = run(cache, docs);

    const auto second = run(cache, docs);

    CHECK(second.ran == counts{0, 0});
    CHECK(second.record == first.record);
}

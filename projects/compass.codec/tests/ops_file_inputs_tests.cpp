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
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
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
#include "compass.store/ops/blob_hash.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"
#include "test_corpus.hpp"

namespace {

using compass::testing::filesystem::scoped_directory;
using namespace compass::codec;

using sources = std::map<std::string, std::string>;
using counts = std::vector<std::uint64_t>;

/**
 * @brief Writes one file from the story it looks up by path, the template
 * file it reads, the paths it lists under data/ that end in .txt and
 * whether data/flag exists. It never enumerates the documents.
 */
void assemble(const types::corpus_view& v, types::file_output& out) {
    std::string text;
    if (const auto story = v.find_path("story.org"))
        text += story->source;
    text += v.read_file("data/template.txt").value_or("<no template>");
    for (const auto p : v.list_files("data/", ".txt"))
        text += std::string(p) + "\n";
    text += v.file_exists("data/flag") ? "flag\n" : "no flag\n";
    out.files.add("out.txt", text);
}

constexpr types::descriptor assembler{.name = "assemble", .version = 1,
    .summary = "", .reads = types::inputs::every_document(), .options = {},
    .rules = {}, .enabled_by_default = true,
    .run = types::corpus_function<types::file_output>{assemble}};

sources documents() {
    return {{"story.org", "#+type: story\n* Goal\n"},
            {"task.org", "#+type: task\n* Goal\n"}};
}

sources files() {
    return {{"data/template.txt", "hello\n"},
            {"data/other.txt", "x\n"},
            {"data/image.png", "png"},
            {"src/main.cpp", "int main() {}\n"}};
}

/**
 * @brief A corpus of @p docs whose checkout also holds @p plain, with the
 * documents among the files as a host lists them.
 */
class checkout final {
public:
    checkout(const sources& docs, const sources& plain) {
        for (const auto& [path, source] : docs) {
            corpus_.add(path, source);
            bytes_[path] = source;
        }
        for (const auto& [path, source] : plain)
            bytes_[path] = source;
        for (const auto& [path, source] : bytes_)
            files_.paths.push_back(path);
        files_.content_of = [this](std::string_view path) {
            return std::optional(compass::store::ops::blob_id_of(
                bytes_.find(path)->second));
        };
        files_.read = [this](std::string_view path) {
            return std::optional(bytes_.find(path)->second);
        };
        view_ = corpus_.corpus();
        view_.files = &files_;
    }
    checkout(const checkout&) = delete;
    checkout& operator=(const checkout&) = delete;

    [[nodiscard]] const types::corpus& corpus() const { return view_; }
    [[nodiscard]] const auto& types() const { return corpus_.types(); }

private:
    tests::test_corpus corpus_;
    std::map<std::string, std::string, std::less<>> bytes_;
    types::checkout_files files_;
    types::corpus view_;
};

struct outcome {
    counts ran;
    counts skipped;
    counts cached;
    std::string output;
    types::dependency_record record;
    bool same_record = false;
};

/**
 * @brief Runs assemble and document_index over a checkout of @p docs and
 * @p plain from @p previous and, when @p cache is not empty, the result
 * cache under it, where it then stores what ran. Checks the outputs equal
 * those of a run with neither.
 */
outcome run(const sources& docs, const sources& plain,
    const types::dependency_record& previous = {},
    const std::filesystem::path& cache = {}) {
    const checkout c(docs, plain);
    const std::array<types::descriptor, 2> codecs{assembler,
        codecs::document_index};
    const auto r = ops::make_registry(codecs, c.types(), {});
    auto result = ops::run(r, c.corpus(), c.types(), 2, previous,
        cache.empty() ? types::action_source{} : io::action_source_of(cache));
    if (!cache.empty())
        (void)io::store_actions(cache, result.record, result.fresh, 2);
    const auto plain_run = ops::run(r, c.corpus(), c.types(), 2);
    REQUIRE(result.files[0].size() == 1);
    CHECK(result.files[0].contents[{0}] ==
        plain_run.files[0].contents[{0}]);
    return {result.ran, result.skipped, result.cached,
        std::string(result.files[0].contents[{0}]),
        std::move(result.record), result.same_record};
}

}

TEST_CASE("a corpus codec reads files, listings and existence of the "
    "checkout", "[ops][file_inputs]") {
    const auto r = run(documents(), files());
    CHECK(r.output == "#+type: story\n* Goal\nhello\n"
        "data/other.txt\ndata/template.txt\nno flag\n");
}

TEST_CASE("a corpus codec that only looked documents up is skipped when "
    "another document changes", "[ops][file_inputs]") {
    const auto first = run(documents(), files());
    auto docs = documents();
    docs["task.org"] += "More words.\n";

    const auto second = run(docs, files(), first.record);

    CHECK(second.ran == counts{0, 1});
    CHECK(second.skipped == counts{1, 0});
}

TEST_CASE("a corpus codec runs again when the document it looked up "
    "changes", "[ops][file_inputs]") {
    const auto first = run(documents(), files());
    auto docs = documents();
    docs["story.org"] += "More words.\n";

    CHECK(run(docs, files(), first.record).ran == counts{1, 1});
}

TEST_CASE("a corpus codec runs again when a file it read changes and not "
    "when an unread file does", "[ops][file_inputs]") {
    const auto first = run(documents(), files());
    auto edited = files();
    edited["data/template.txt"] = "bye\n";
    CHECK(run(documents(), edited, first.record).ran == counts{1, 0});

    auto unread = files();
    unread["src/main.cpp"] += "// more\n";
    unread["data/other.txt"] = "listed, never read\n";
    CHECK(run(documents(), unread, first.record).ran == counts{0, 0});
}

TEST_CASE("a corpus codec runs again when a file it read appears or goes",
    "[ops][file_inputs]") {
    auto none = files();
    none.erase("data/template.txt");
    const auto first = run(documents(), none);
    CHECK(first.output.find("<no template>") != std::string::npos);

    CHECK(run(documents(), files(), first.record).ran == counts{1, 0});
}

TEST_CASE("a corpus codec runs again when its listing gains or loses a "
    "match and not for a file outside it", "[ops][file_inputs]") {
    const auto first = run(documents(), files());
    auto added = files();
    added["data/new.txt"] = "";
    CHECK(run(documents(), added, first.record).ran == counts{1, 0});

    auto removed = files();
    removed.erase("data/other.txt");
    CHECK(run(documents(), removed, first.record).ran == counts{1, 0});

    auto outside = files();
    outside["data/new.png"] = "";
    outside["data.txt"] = "";
    outside["datum/x.txt"] = "";
    CHECK(run(documents(), outside, first.record).ran == counts{0, 0});
}

TEST_CASE("a file tested for existence runs the codec again when it "
    "appears, not when its bytes change", "[ops][file_inputs]") {
    const auto first = run(documents(), files());
    auto flagged = files();
    flagged["data/flag"] = "one";
    const auto second = run(documents(), flagged, first.record);
    CHECK(second.ran == counts{1, 0});
    CHECK(second.output.ends_with("\nflag\n"));

    flagged["data/flag"] = "two";
    CHECK(run(documents(), flagged, second.record).ran == counts{0, 0});
}

TEST_CASE("a corpus codec that enumerates runs again for any document",
    "[ops][file_inputs]") {
    const auto first = run(documents(), files());
    auto docs = documents();
    docs["task.org"] += "More words.\n";

    const auto second = run(docs, files(), first.record);

    CHECK(second.ran[1] == 1);
    CHECK(second.skipped[1] == 0);
}

TEST_CASE("another worktree whose other documents differ takes a corpus "
    "unit that only looked up from the cache", "[ops][file_inputs]") {
    const scoped_directory cache("compass.codec.file_inputs");
    const auto first = run(documents(), files(), {}, cache.path());
    auto docs = documents();
    docs["task.org"] += "Only in this worktree.\n";
    auto plain = files();
    plain["src/main.cpp"] += "// and this\n";

    const auto second = run(docs, plain, {}, cache.path());

    CHECK(second.cached == counts{1, 0});
    CHECK(second.ran == counts{0, 1});
    CHECK(second.output == first.output);
}

TEST_CASE("file inputs survive the record's encoding", "[ops][file_inputs]") {
    const auto first = run(documents(), files());
    const auto bytes = ops::encode_dependency_record(first.record);
    const auto decoded = ops::decode_dependency_record(bytes);
    CHECK(ops::encode_dependency_record(decoded) == bytes);

    std::vector<types::input_kind> kinds(decoded.input_kinds.values.begin(),
        decoded.input_kinds.values.end());
    CHECK(std::ranges::count(kinds, types::input_kind::file) == 1);
    CHECK(std::ranges::count(kinds, types::input_kind::file_exists) == 1);
    CHECK(std::ranges::count(kinds, types::input_kind::listing) == 1);
    CHECK(std::ranges::count(kinds, types::input_kind::path) == 1);
    CHECK(std::ranges::count(kinds, types::input_kind::type_set) == 1);
}

TEST_CASE("a run is said to keep the record only when the record stood "
    "in for every unit and held no other", "[ops][file_inputs]") {
    const auto first = run(documents(), files());
    CHECK_FALSE(first.same_record);
    CHECK(run(documents(), files(), first.record).same_record);

    auto edited = files();
    edited["data/template.txt"] = "bye\n";
    CHECK_FALSE(run(documents(), edited, first.record).same_record);

    auto larger = first.record;
    larger.codecs.add("gone");
    larger.versions.values.push_back(1);
    larger.configurations.values.push_back({});
    larger.documents.add("");
    larger.input_firsts.values.push_back(
        {static_cast<std::uint32_t>(larger.input_count())});
    larger.file_firsts.values.push_back(
        {static_cast<std::uint32_t>(larger.files.size())});
    larger.diagnostic_counts.values.push_back(0);
    larger.diagnostic_hashes.values.push_back({});
    larger.diagnostics.add("");
    larger.manifests.values.push_back({});
    larger.results.values.push_back({});
    CHECK_FALSE(run(documents(), files(), larger).same_record);
}

TEST_CASE("a listing prefix with a star is refused", "[ops][file_inputs]") {
    CHECK_THROWS_AS(types::listing_key("data/*", ".txt"),
        std::invalid_argument);
    CHECK(types::listing_key("data/", ".txt") == "data/*.txt");
}

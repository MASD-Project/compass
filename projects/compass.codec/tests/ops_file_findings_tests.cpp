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
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include <catch2/catch_test_macros.hpp>
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

using sources = std::map<std::string, std::string, std::less<>>;
using counts = std::vector<std::uint64_t>;

constexpr std::array<types::rule, 1> rules{{{"bad", "No file says bad."}}};

/**
 * @brief Reports each .txt file under data/ that holds "bad", at the word,
 * and the story when it holds "bad".
 */
void flag(const types::corpus_view& v, types::diagnostic_table& out) {
    for (const auto path : v.list_files("data/", ".txt")) {
        const auto text = v.read_file(path).value_or("");
        if (const auto at = text.find("bad"); at != std::string::npos) {
            const auto begin = static_cast<std::uint32_t>(at);
            out.add({compass::org::types::document_id::none(),
                {begin, begin + 3}, types::rule_id{0}, types::severity::error,
                "the file says bad", "say good", path});
        }
    }
    if (const auto story = v.find_path("story.org");
        story && story->source.find("bad") != std::string_view::npos)
        out.add({story->row, {0, 1}, types::rule_id{0},
            types::severity::error, "the story says bad", "say good", {}});
}

constexpr types::descriptor flagger{.name = "flag", .version = 1,
    .summary = "", .reads = types::inputs::every_document(), .options = {},
    .rules = rules, .enabled_by_default = true,
    .run = types::corpus_function<types::diagnostic_table>{flag}};

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
    sources bytes_;
    types::checkout_files files_;
    types::corpus view_;
};

/**
 * @brief A finding as the path it names, whether that is a file, its
 * range and its message.
 */
using finding = std::tuple<std::string, bool, std::uint32_t, std::uint32_t,
    std::string>;

std::vector<finding> findings_of(const types::corpus& c,
    const types::diagnostic_table& t) {
    std::vector<finding> out;
    for (std::uint32_t i = 0; i < t.size(); ++i) {
        const auto d = t[types::diagnostic_id{i}];
        const auto is_file = d.document.is_none();
        out.emplace_back(std::string(is_file ? d.file
                                             : c.paths[d.document.value]),
            is_file, d.range.begin, d.range.end, std::string(d.message));
    }
    return out;
}

struct outcome {
    counts ran;
    counts skipped;
    counts cached;
    std::vector<finding> findings;
    types::dependency_record record;
};

outcome run(const sources& plain, const types::dependency_record& previous
    = {}, const std::filesystem::path& cache = {}) {
    const checkout c({{"story.org", "#+type: story\n* Goal\nbad\n"}}, plain);
    const std::array<types::descriptor, 1> codecs{flagger};
    const auto r = ops::make_registry(codecs, c.types(), {});
    auto result = ops::run(r, c.corpus(), c.types(), 2, previous,
        cache.empty() ? types::action_source{} : io::action_source_of(cache));
    if (!cache.empty())
        (void)io::store_actions(cache, result.record, result.fresh, 2);
    return {result.ran, result.skipped, result.cached,
        findings_of(c.corpus(), result.diagnostics[0]),
        std::move(result.record)};
}

sources plain() {
    return {{"data/a.txt", "all bad\n"}, {"data/b.txt", "fine\n"},
            {"data/c.txt", "bad\n"}};
}

std::vector<finding> expected() {
    return {{"data/a.txt", true, 4, 7, "the file says bad"},
            {"data/c.txt", true, 0, 3, "the file says bad"},
            {"story.org", false, 0, 1, "the story says bad"}};
}

}

TEST_CASE("a finding about a file that is not a document names the file",
    "[ops][file_findings]") {
    CHECK(run(plain()).findings == expected());
}

TEST_CASE("a unit the record stands in for gives back its file findings",
    "[ops][file_findings]") {
    const auto first = run(plain());
    const auto second = run(plain(), first.record);
    CHECK(second.skipped == counts{1});
    CHECK(second.findings == expected());
}

TEST_CASE("a unit taken from the cache gives back its file findings",
    "[ops][file_findings]") {
    const scoped_directory cache("compass.codec.file_findings");
    (void)run(plain(), {}, cache.path());
    const auto second = run(plain(), {}, cache.path());
    CHECK(second.cached == counts{1});
    CHECK(second.findings == expected());
}

TEST_CASE("file findings survive the record's encoding",
    "[ops][file_findings]") {
    const auto first = run(plain());
    const auto bytes = ops::encode_dependency_record(first.record);
    const auto decoded = ops::decode_dependency_record(bytes);
    CHECK(ops::encode_dependency_record(decoded) == bytes);
    const auto second = run(plain(), decoded);
    CHECK(second.skipped == counts{1});
    CHECK(second.findings == expected());
}

TEST_CASE("a unit runs again when a file it reported goes",
    "[ops][file_findings]") {
    const auto first = run(plain());
    auto fewer = plain();
    fewer.erase("data/c.txt");
    const auto second = run(fewer, first.record);
    CHECK(second.ran == counts{1});
    CHECK(second.findings == std::vector<finding>{expected()[0],
        expected()[2]});
}

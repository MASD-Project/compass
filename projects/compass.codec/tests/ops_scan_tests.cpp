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
#include <atomic>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/io/action_cache.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/configuration.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"
#include "test_corpus.hpp"

namespace {

using compass::testing::filesystem::scoped_directory;
using namespace compass::codec;

using sources = std::map<std::string, std::string>;

std::atomic<int> mapped{0};
std::atomic<int> reduced{0};

constexpr std::array<types::rule, 2> rules{{
    {"bad_line", "No line of a file is the option marker."},
    {"defined_twice", "No two files define one name."}}};

constexpr std::array<types::option, 1> options{{
    {"marker", "bad", "The line that is a finding."}}};

std::vector<std::string_view> scanned(const types::corpus_view& v) {
    return v.list_files("src/", ".txt");
}

/**
 * @brief Reports each line that is the marker, and makes the fact the
 * names the file defines, one per line, from its lines "def <name>".
 */
void map_file(std::string_view path, std::string_view bytes,
    const types::option_values& o, types::file_scan& out) {
    ++mapped;
    std::size_t at = 0;
    while (at < bytes.size()) {
        auto end = bytes.find('\n', at);
        if (end == std::string_view::npos)
            end = bytes.size();
        const auto line = bytes.substr(at, end - at);
        const auto offset = static_cast<std::uint32_t>(at);
        if (line == o["marker"])
            out.findings.add({.document =
                    compass::org::types::document_id::none(),
                .range = {offset, offset}, .rule = types::rule_id{0},
                .level = types::severity::error,
                .message = "the marker line", .fix = "remove it",
                .file = path});
        if (line.starts_with("def "))
            out.fact += std::string(line.substr(4)) + "\n";
        at = end + 1;
    }
}

/**
 * @brief Reports, on the later file, each name two files define that
 * src/ignore.cfg does not list, at the line that defines it, which is
 * found by reading the file.
 */
void reduce_files(std::span<const types::scanned_file> files,
    const types::corpus_view& v, types::diagnostic_table& out) {
    ++reduced;
    const auto ignored = "\n" + v.read_file("src/ignore.cfg").value_or("");
    std::map<std::string, std::string_view> first;
    for (const auto& f : files) {
        auto fact = f.fact;
        while (!fact.empty()) {
            const auto end = fact.find('\n');
            const std::string name(fact.substr(0, end));
            fact.remove_prefix(end + 1);
            const auto [it, added] = first.try_emplace(name, f.path);
            if (added || ignored.contains("\n" + name + "\n"))
                continue;
            const auto message = name + " is also defined in " +
                std::string(it->second);
            const auto bytes = v.read_file(f.path).value_or("");
            const auto at = static_cast<std::uint32_t>(
                bytes.find("def " + name));
            out.add({.document = compass::org::types::document_id::none(),
                .range = {at, at}, .rule = types::rule_id{1},
                .level = types::severity::error, .message = message,
                .fix = "rename one", .file = f.path});
        }
    }
}

constexpr types::descriptor names{.name = "names", .version = 1,
    .summary = "", .reads = types::inputs::of_types({}), .options = options,
    .rules = rules, .enabled_by_default = true,
    .run = types::scan_function{scanned, map_file, reduce_files}};

/**
 * @brief A corpus with no documents whose checkout holds @p plain.
 */
class checkout final {
public:
    explicit checkout(const sources& plain)
        : bytes_(plain.begin(), plain.end()) {
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

/**
 * @brief Each finding as its file, rule, offset and message, in order.
 */
std::vector<std::string> lines_of(const types::diagnostic_table& t) {
    std::vector<std::string> out;
    for (std::uint32_t i = 0; i < t.size(); ++i) {
        const types::diagnostic_id d{i};
        out.push_back(std::string(t.files[d]) + " " +
            std::to_string(t.rules[d].value) + " " +
            std::to_string(t.ranges[d].begin) + " " +
            std::string(t.messages[d]));
    }
    return out;
}

struct outcome {
    std::vector<std::string> findings;
    int maps = 0;
    int reduces = 0;
    types::dependency_record record;
};

/**
 * @brief Runs names over @p plain from @p previous and the cache under
 * @p cache, then stores what ran there. Checks the findings equal those of
 * a run with neither, which maps every file.
 */
outcome run(const sources& plain, const scoped_directory& cache,
    const types::dependency_record& previous = {},
    const types::configuration& settings = {}) {
    const checkout c(plain);
    const std::array<types::descriptor, 1> codecs{names};
    const auto r = ops::make_registry(codecs, c.types(), settings);
    mapped = 0;
    reduced = 0;
    auto result = ops::run(r, c.corpus(), c.types(), 2, previous,
        io::action_source_of(cache.path()));
    const auto maps = mapped.load();
    const auto reduces = reduced.load();
    (void)io::store_actions(cache.path(), result.record, result.fresh, 2,
        result.scans);
    const auto plain_run = ops::run(r, c.corpus(), c.types(), 2);
    CHECK(lines_of(result.diagnostics[0]) ==
        lines_of(plain_run.diagnostics[0]));
    return {lines_of(result.diagnostics[0]), maps, reduces,
        std::move(result.record)};
}

sources tree() {
    return {{"src/a.txt", "def alpha\nbad\n"},
            {"src/b.txt", "def beta\n"},
            {"src/c.txt", "def gamma\n"},
            {"src/notes.md", "def alpha\n"}};
}

}

TEST_CASE("a scan maps every listed file, then reports each file's "
    "findings and those of the reduce", "[ops][scan]") {
    const scoped_directory cache("compass.codec.scan");
    auto files = tree();
    files["src/c.txt"] += "def beta\n";

    const auto r = run(files, cache);

    CHECK(r.maps == 3);
    CHECK(r.findings == std::vector<std::string>{
        "src/a.txt 0 10 the marker line",
        "src/c.txt 1 10 beta is also defined in src/b.txt"});
}

TEST_CASE("after an edit a scan maps only the edited file, and its reduce "
    "sees the new fact beside the stored ones", "[ops][scan]") {
    const scoped_directory cache("compass.codec.scan");
    const auto first = run(tree(), cache);
    REQUIRE(first.maps == 3);
    auto files = tree();
    files["src/c.txt"] = "def alpha\nbad\n";

    const auto second = run(files, cache, first.record);

    CHECK(second.maps == 1);
    CHECK(second.findings == std::vector<std::string>{
        "src/a.txt 0 10 the marker line",
        "src/c.txt 0 10 the marker line",
        "src/c.txt 1 0 alpha is also defined in src/a.txt"});

    files["src/c.txt"] = "def delta\n";
    const auto third = run(files, cache, second.record);
    CHECK(third.maps == 1);
    CHECK(third.findings == std::vector<std::string>{
        "src/a.txt 0 10 the marker line"});
}

TEST_CASE("a scan forgets a removed file and maps a new one",
    "[ops][scan]") {
    const scoped_directory cache("compass.codec.scan");
    auto files = tree();
    files["src/d.txt"] = "def beta\n";
    const auto first = run(files, cache);
    REQUIRE(first.findings.size() == 2);

    files.erase("src/b.txt");
    files["src/e.txt"] = "def gamma\n";
    const auto second = run(files, cache, first.record);

    CHECK(second.maps == 1);
    CHECK(second.findings == std::vector<std::string>{
        "src/a.txt 0 10 the marker line",
        "src/e.txt 1 0 gamma is also defined in src/c.txt"});
}

TEST_CASE("a scan whose options changed maps every file again",
    "[ops][scan]") {
    const scoped_directory cache("compass.codec.scan");
    const auto first = run(tree(), cache);
    types::configuration settings;
    settings.set("names", "marker", "def beta");

    const auto second = run(tree(), cache, first.record, settings);

    CHECK(second.maps == 3);
    CHECK(second.findings == std::vector<std::string>{
        "src/b.txt 0 0 the marker line"});
}

TEST_CASE("a scan whose result object is gone maps every file again",
    "[ops][scan]") {
    std::vector<std::string> findings;
    types::dependency_record record;
    {
        const scoped_directory cache("compass.codec.scan");
        auto first = run(tree(), cache);
        findings = first.findings;
        record = std::move(first.record);
    }
    const scoped_directory empty("compass.codec.scan");
    auto files = tree();
    files["src/b.txt"] += "\n";

    const auto second = run(files, empty, record);

    CHECK(second.maps == 3);
    CHECK(second.findings == findings);
}

TEST_CASE("an edit that leaves a file's fact as it was is mapped and not "
    "reduced", "[ops][scan]") {
    const scoped_directory cache("compass.codec.scan");
    auto files = tree();
    files["src/c.txt"] = "def alpha\n";
    const auto first = run(files, cache);
    REQUIRE(first.reduces == 1);
    files["src/b.txt"] = "a note\ndef beta\nbad\n";

    const auto second = run(files, cache, first.record);

    CHECK(second.maps == 1);
    CHECK(second.reduces == 0);
    CHECK(second.findings == std::vector<std::string>{
        "src/a.txt 0 10 the marker line",
        "src/b.txt 0 16 the marker line",
        "src/c.txt 1 0 alpha is also defined in src/a.txt"});

    files["src/b.txt"] = "def beta\ndef alpha\n";
    const auto third = run(files, cache, second.record);
    CHECK(third.reduces == 1);
}

TEST_CASE("an edit to a file the reduce step read is reduced again though "
    "its fact is as it was", "[ops][scan]") {
    const scoped_directory cache("compass.codec.scan");
    auto files = tree();
    files["src/c.txt"] = "def alpha\n";
    const auto first = run(files, cache);
    files["src/c.txt"] = "a note\ndef alpha\n";

    const auto second = run(files, cache, first.record);

    CHECK(second.reduces == 1);
    CHECK(second.findings.back() ==
        "src/c.txt 1 7 alpha is also defined in src/a.txt");
}

TEST_CASE("a scan that was not reduced still depends on what its reduce "
    "step read", "[ops][scan]") {
    const scoped_directory cache("compass.codec.scan");
    auto files = tree();
    files["src/c.txt"] = "def alpha\n";
    const auto first = run(files, cache);
    files["src/b.txt"] = "a note\ndef beta\n";
    const auto second = run(files, cache, first.record);
    REQUIRE(second.reduces == 0);
    files["src/ignore.cfg"] = "alpha\n";

    const auto third = run(files, cache, second.record);

    CHECK(third.maps == 0);
    CHECK(third.reduces == 1);
    CHECK(third.findings == std::vector<std::string>{
        "src/a.txt 0 10 the marker line"});
}

TEST_CASE("a file that comes or goes is reduced again", "[ops][scan]") {
    const scoped_directory cache("compass.codec.scan");
    const auto first = run(tree(), cache);
    auto files = tree();
    files["src/d.txt"] = "a note\n";

    CHECK(run(files, cache, first.record).reduces == 1);
}

namespace {

std::atomic<int> pages_mapped{0};

constexpr std::array<types::rule, 2> heading_rules{{
    {"bad_heading", "No heading is titled bad."},
    {"heading_twice", "No two pages share a heading."}}};

std::vector<std::string_view> org_pages(const types::corpus_view& v) {
    return v.list_files("doc/", ".org");
}

/**
 * @brief Reports each heading titled bad, from the parsed headings, and
 * makes the fact the page's heading titles, one per line.
 */
void map_headings(const types::document& d, const types::option_values&,
    types::file_scan& out) {
    ++pages_mapped;
    const auto& t = d.tables;
    for (std::uint32_t i = 0; i < t.headings.size(); ++i) {
        const compass::org::types::heading_id h{i};
        const auto title = t.headings.titles[h].of(d.source);
        const auto begin = t.elements.ranges[t.headings.elements[h]].begin;
        if (title == "bad")
            out.findings.add({.document = d.row, .range = {begin, begin},
                .rule = types::rule_id{0}, .level = types::severity::error,
                .message = "a bad heading", .fix = "rename it"});
        out.fact += std::string(title) + "\n";
    }
}

/**
 * @brief Reports, on the later page, each heading title an earlier page
 * has.
 */
void reduce_headings(std::span<const types::scanned_file> files,
    const types::corpus_view&, types::diagnostic_table& out) {
    ++reduced;
    std::map<std::string, std::string_view> first;
    for (const auto& f : files) {
        auto fact = f.fact;
        while (!fact.empty()) {
            const auto end = fact.find('\n');
            const std::string title(fact.substr(0, end));
            fact.remove_prefix(end + 1);
            const auto [it, added] = first.try_emplace(title, f.path);
            if (added)
                continue;
            out.add({.document = compass::org::types::document_id::none(),
                .range = {0, 0}, .rule = types::rule_id{1},
                .level = types::severity::error,
                .message = title + " is also a heading of " +
                    std::string(it->second),
                .fix = "rename one", .file = f.path});
        }
    }
}

constexpr types::descriptor headings{.name = "headings", .version = 1,
    .summary = "", .reads = types::inputs::of_types({}), .options = {},
    .rules = heading_rules, .enabled_by_default = true,
    .run = types::scan_function{.files = org_pages,
        .reduce = reduce_headings, .map_document = map_headings}};

/**
 * @brief A corpus whose .org files are parsed documents, and whose
 * checkout holds every file.
 */
class org_checkout final {
public:
    explicit org_checkout(const sources& all)
        : bytes_(all.begin(), all.end()) {
        for (const auto& [path, source] : bytes_) {
            files_.paths.push_back(path);
            if (path.ends_with(".org"))
                corpus_.add(path, source);
        }
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
    org_checkout(const org_checkout&) = delete;
    org_checkout& operator=(const org_checkout&) = delete;

    [[nodiscard]] const types::corpus& corpus() const { return view_; }
    [[nodiscard]] const auto& types() const { return corpus_.types(); }

private:
    tests::test_corpus corpus_;
    std::map<std::string, std::string, std::less<>> bytes_;
    types::checkout_files files_;
    types::corpus view_;
};

/**
 * @brief Each finding as its path, rule, offset and message, in order.
 */
std::vector<std::string> findings_of(const types::diagnostic_table& t,
    const types::corpus& c) {
    std::vector<std::string> out;
    for (std::uint32_t i = 0; i < t.size(); ++i) {
        const types::diagnostic_id d{i};
        const auto document = t.documents[d];
        out.push_back(std::string(document.is_none() ? t.files[d]
                : c.paths[document.value]) + " " +
            std::to_string(t.rules[d].value) + " " +
            std::to_string(t.ranges[d].begin) + " " +
            std::string(t.messages[d]));
    }
    return out;
}

outcome run_headings(const sources& all, const scoped_directory& cache,
    const types::dependency_record& previous = {}) {
    const org_checkout c(all);
    const std::array<types::descriptor, 1> codecs{headings};
    const auto r = ops::make_registry(codecs, c.types(), {});
    pages_mapped = 0;
    reduced = 0;
    auto result = ops::run(r, c.corpus(), c.types(), 2, previous,
        io::action_source_of(cache.path()));
    const auto maps = pages_mapped.load();
    const auto reduces = reduced.load();
    (void)io::store_actions(cache.path(), result.record, result.fresh, 2,
        result.scans);
    const auto plain_run = ops::run(r, c.corpus(), c.types(), 2);
    const auto findings = findings_of(result.diagnostics[0], c.corpus());
    CHECK(findings == findings_of(plain_run.diagnostics[0], c.corpus()));
    return {findings, maps, reduces, std::move(result.record)};
}

sources pages() {
    return {{"doc/a.org", "* bad\n* alpha\n"},
            {"doc/b.org", "* beta\n"},
            {"doc/c.org", "text\n* gamma\n"},
            {"doc/notes.txt", "* alpha\n"}};
}

}

TEST_CASE("a scan of documents maps each page from its parsed tables and "
    "reports its findings on the document", "[ops][scan]") {
    const scoped_directory cache("compass.codec.scan");
    auto files = pages();
    files["doc/c.org"] += "* beta\n";

    const auto r = run_headings(files, cache);

    CHECK(r.maps == 3);
    CHECK(r.findings == std::vector<std::string>{
        "doc/a.org 0 0 a bad heading",
        "doc/c.org 1 0 beta is also a heading of doc/b.org"});
}

TEST_CASE("after an edit a scan of documents maps only the edited page, "
    "and reduces only when its fact changed", "[ops][scan]") {
    const scoped_directory cache("compass.codec.scan");
    const auto first = run_headings(pages(), cache);
    REQUIRE(first.maps == 3);
    auto files = pages();
    files["doc/c.org"] = "more text\n* gamma\n";

    const auto second = run_headings(files, cache, first.record);

    CHECK(second.maps == 1);
    CHECK(second.reduces == 0);
    CHECK(second.findings == std::vector<std::string>{
        "doc/a.org 0 0 a bad heading"});

    files["doc/c.org"] = "* alpha\n* bad\n";
    const auto third = run_headings(files, cache, second.record);
    CHECK(third.maps == 1);
    CHECK(third.reduces == 1);
    CHECK(third.findings == std::vector<std::string>{
        "doc/a.org 0 0 a bad heading", "doc/c.org 0 8 a bad heading",
        "doc/c.org 1 0 alpha is also a heading of doc/a.org",
        "doc/c.org 1 0 bad is also a heading of doc/a.org"});

    const auto fourth = run_headings(files, cache, third.record);
    CHECK(fourth.maps == 0);
    CHECK(fourth.findings == third.findings);
}

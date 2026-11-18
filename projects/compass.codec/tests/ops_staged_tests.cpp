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
#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/io/action_cache.hpp"
#include "compass.codec/ops/action_cache.hpp"
#include "compass.codec/ops/dependencies.hpp"
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
using rates = std::map<std::string, std::string, std::less<>>;

std::atomic<int> prepared{0};
std::atomic<int> summed{0};
std::atomic<int> copied{0};

std::string rate_fact(std::string_view key) {
    return "rate" + std::string(1, '\0') + std::string(key);
}

/**
 * @brief Reads the "key=value" lines of rates.txt, publishes one fact per
 * key, and lists every src/ .txt file as a part.
 */
std::shared_ptr<const void> prepare(const types::corpus_view& v,
    types::stage& out) {
    ++prepared;
    auto state = std::make_shared<rates>();
    const auto text = v.read_file("rates.txt").value_or("");
    for (std::size_t at = 0; at < text.size();) {
        const auto end = std::min(text.find('\n', at), text.size());
        const auto line = std::string_view(text).substr(at, end - at);
        const auto eq = line.find('=');
        if (eq != std::string_view::npos) {
            state->emplace(line.substr(0, eq), line.substr(eq + 1));
            out.facts.emplace_back(rate_fact(line.substr(0, eq)),
                compass::store::types::blob_id{
                    compass::store::ops::sha1_of(line.substr(eq + 1))});
        }
        at = end + 1;
    }
    for (const auto path : v.list_files("src/", ".txt"))
        out.parts.emplace_back(path);
    return state;
}

/**
 * @brief For the part src/<name>.txt, whose lines are "use <key>", emits
 * out/<name>.sum with each key and its rate, and out/<name>.side with the
 * bytes of src/<name>.side. When only rates changed since its last call,
 * the side file is kept and src/<name>.side is not read.
 */
void run_parts(const void* state, std::span<const std::string_view> parts,
    const types::corpus_view& v, types::part_sink& out) {
    const auto& known = *static_cast<const rates*>(state);
    for (const auto part : parts) {
        out.begin(part);
        ++summed;
        const auto stem = std::string(part.substr(4, part.size() - 8));
        const auto text = v.read_file(part).value_or("");
        std::string sum;
        for (std::size_t at = 0; at < text.size();) {
            const auto end = std::min(text.find('\n', at), text.size());
            const auto line = std::string_view(text).substr(at, end - at);
            if (line.starts_with("use ")) {
                const auto key = line.substr(4);
                out.read(rate_fact(key));
                const auto it = known.find(key);
                sum += std::string(key) + "=" +
                    (it == known.end() ? "?" : it->second) + "\n";
            }
            at = end + 1;
        }
        out.add("out/" + stem + ".sum", sum);

        const auto side = "out/" + stem + ".side";
        const auto changed = out.changed();
        const bool rates_only = changed && std::ranges::all_of(*changed,
            [](const types::changed_input& c) {
                return c.kind == types::input_kind::fact;
            });
        if (rates_only && out.keep(side))
            continue;
        ++copied;
        out.add(side, v.read_file("src/" + stem + ".side").value_or(""));
    }
}

constexpr types::descriptor sheets{.name = "sheets", .version = 1,
    .summary = "", .reads = types::inputs::of_types({}), .options = {},
    .rules = {}, .enabled_by_default = true,
    .run = types::staged_function{prepare, run_parts}};

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

std::vector<std::pair<std::string, std::string>> files_of(
    const types::file_table& t) {
    std::vector<std::pair<std::string, std::string>> out;
    for (std::uint32_t f = 0; f < t.size(); ++f)
        out.emplace_back(t.paths[{f}], t.contents[{f}]);
    return out;
}

struct outcome {
    std::vector<std::pair<std::string, std::string>> files;
    std::set<std::string> ran;
    std::uint64_t skipped = 0;
    std::uint64_t cached = 0;
    int prepares = 0;
    int sums = 0;
    int copies = 0;
    types::dependency_record record;
};

/**
 * @brief Runs sheets over @p plain from @p previous and the cache under
 * @p cache, then stores what ran there. Checks that the files, and the
 * record but for its cache keys' order of inputs, are those of a run with
 * neither, which calls every part.
 */
outcome run(const sources& plain, const scoped_directory& cache,
    const types::dependency_record& previous = {}) {
    const checkout c(plain);
    const std::array<types::descriptor, 1> codecs{sheets};
    const auto r = ops::make_registry(codecs, c.types(), {});
    prepared = summed = copied = 0;
    auto result = ops::run(r, c.corpus(), c.types(), 2, previous,
        io::action_source_of(cache.path()));
    outcome o;
    o.prepares = prepared;
    o.sums = summed;
    o.copies = copied;
    (void)io::store_actions(cache.path(), result.record, result.fresh, 2,
        result.scans);
    const auto whole = ops::run(r, c.corpus(), c.types(), 2);
    CHECK(files_of(result.files[0]) == files_of(whole.files[0]));
    REQUIRE(result.record.size() == whole.record.size());
    CHECK(result.record.file_hashes.values ==
        whole.record.file_hashes.values);
    for (std::uint32_t u = 0; u < whole.record.size(); ++u) {
        INFO(whole.record.documents[{u}]);
        CHECK(result.record.documents[{u}] == whole.record.documents[{u}]);
        CHECK(result.record.results[{u}] == whole.record.results[{u}]);
    }
    for (const auto u : result.fresh)
        o.ran.emplace(result.record.documents[u]);
    o.files = files_of(result.files[0]);
    o.skipped = result.skipped[0];
    o.cached = result.cached[0];
    o.record = std::move(result.record);
    return o;
}

sources tree() {
    return {{"rates.txt", "eur=2\nusd=3\n"},
            {"src/a.txt", "use eur\n"},
            {"src/a.side", "side of a\n"},
            {"src/b.txt", "use usd\nuse gbp\n"},
            {"src/b.side", "side of b\n"},
            {"src/c.txt", "use eur\nuse usd\n"},
            {"notes.md", "nothing\n"}};
}

types::unit_index index_of(const types::dependency_record& record) {
    const auto bytes = ops::encode_dependency_record(record);
    const auto header = std::string_view(bytes).substr(0,
        ops::record_header_size);
    const auto layout = ops::locate_unit_index(header,
        std::string_view(bytes).substr(bytes.size() -
            ops::record_footer_size), bytes.size());
    return ops::decode_unit_index(header, std::string_view(bytes).substr(
        layout.index_offset, layout.index_size), layout);
}

std::set<std::string> touched(const types::dependency_record& record,
    std::string_view path, bool created = false) {
    const ops::edited_file file{.path = path, .output = path,
        .document = false, .created = created, .ids = {}};
    std::set<std::string> out;
    for (const auto u : ops::units_touching(index_of(record), file))
        out.emplace(record.documents[u]);
    return out;
}

}

TEST_CASE("a staged codec emits the files of every part, in the order "
    "prepare listed them", "[ops][staged]") {
    const scoped_directory cache("compass.codec.staged");

    const auto r = run(tree(), cache);

    CHECK(r.files == std::vector<std::pair<std::string, std::string>>{
        {"out/a.sum", "eur=2\n"}, {"out/a.side", "side of a\n"},
        {"out/b.sum", "usd=3\ngbp=?\n"}, {"out/b.side", "side of b\n"},
        {"out/c.sum", "eur=2\nusd=3\n"}, {"out/c.side", ""}});
    CHECK(r.ran == std::set<std::string>{"", "src/a.txt", "src/b.txt",
        "src/c.txt"});
}

TEST_CASE("a second run calls neither prepare nor a part",
    "[ops][staged]") {
    const scoped_directory cache("compass.codec.staged");
    const auto first = run(tree(), cache);

    const auto second = run(tree(), cache, first.record);

    CHECK(second.ran.empty());
    CHECK(second.skipped == 4);
    CHECK(second.prepares == 0);
}

TEST_CASE("a fact that changes runs only the parts that read it",
    "[ops][staged]") {
    const scoped_directory cache("compass.codec.staged");
    const auto first = run(tree(), cache);
    auto files = tree();
    files["rates.txt"] = "eur=2\nusd=4\n";

    const auto second = run(files, cache, first.record);

    CHECK(second.ran == std::set<std::string>{"", "src/b.txt", "src/c.txt"});
    CHECK(second.sums == 2);
    CHECK(second.files[2].second == "usd=4\ngbp=?\n");
}

TEST_CASE("a fact a part looked up and missed runs the part when it "
    "appears", "[ops][staged]") {
    const scoped_directory cache("compass.codec.staged");
    const auto first = run(tree(), cache);
    auto files = tree();
    files["rates.txt"] += "gbp=5\n";

    const auto second = run(files, cache, first.record);

    CHECK(second.ran == std::set<std::string>{"", "src/b.txt"});
    CHECK(second.files[2].second == "usd=3\ngbp=5\n");
}

TEST_CASE("an edit to what one part reads runs that part, with prepare "
    "called only for its state", "[ops][staged]") {
    const scoped_directory cache("compass.codec.staged");
    const auto first = run(tree(), cache);
    auto files = tree();
    files["src/a.side"] = "another side\n";

    const auto second = run(files, cache, first.record);

    CHECK(second.ran == std::set<std::string>{"src/a.txt"});
    CHECK(second.prepares == 1);
    CHECK(second.sums == 1);
}

TEST_CASE("a part that goes takes its files, and a new one is called",
    "[ops][staged]") {
    const scoped_directory cache("compass.codec.staged");
    const auto first = run(tree(), cache);
    auto files = tree();
    files.erase("src/a.txt");
    files["src/d.txt"] = "use eur\n";

    const auto second = run(files, cache, first.record);

    CHECK(second.ran == std::set<std::string>{"", "src/d.txt"});
    CHECK(second.files.front().first == "out/b.sum");
    CHECK(second.files.back() ==
        std::pair<std::string, std::string>{"out/d.side", ""});
}

TEST_CASE("a part keeps a file its changed inputs cannot alter, and still "
    "depends on what producing it read", "[ops][staged]") {
    const scoped_directory cache("compass.codec.staged");
    const auto first = run(tree(), cache);
    REQUIRE(first.copies == 3);
    auto files = tree();
    files["rates.txt"] = "eur=2\nusd=4\n";

    const auto second = run(files, cache, first.record);

    CHECK(second.sums == 2);
    CHECK(second.copies == 0);

    files["src/b.side"] = "a new side of b\n";
    const auto third = run(files, cache, second.record);
    CHECK(third.ran == std::set<std::string>{"src/b.txt"});
    CHECK(third.copies == 1);
    CHECK(third.files[3].second == "a new side of b\n");
}

TEST_CASE("the parts of a staged codec come from the cache in a checkout "
    "with no record", "[ops][staged]") {
    const scoped_directory cache("compass.codec.staged");
    (void)run(tree(), cache);

    const auto again = run(tree(), cache);

    CHECK(again.ran.empty());
    CHECK(again.cached == 4);
    CHECK(again.prepares == 0);
}

TEST_CASE("a filter that takes one part runs it and leaves the others out",
    "[ops][staged]") {
    const scoped_directory cache("compass.codec.staged");
    const auto first = run(tree(), cache);
    auto files = tree();
    files["src/a.side"] = "another side\n";
    const checkout c(files);
    const std::array<types::descriptor, 1> codecs{sheets};
    const auto r = ops::make_registry(codecs, c.types(), {});

    const auto result = ops::run(r, c.corpus(), c.types(), 2, first.record,
        io::action_source_of(cache.path()),
        [](types::codec_id, std::string_view document) {
            return document.empty() || document == "src/a.txt";
        });

    CHECK(result.record.size() == 2);
    CHECK(files_of(result.files[0]) ==
        std::vector<std::pair<std::string, std::string>>{
            {"out/a.sum", "eur=2\n"}, {"out/a.side", "another side\n"}});
}

TEST_CASE("an edit to what prepare or a part reads names every unit of "
    "the staged codec", "[ops][staged][unit_index]") {
    const scoped_directory cache("compass.codec.staged");
    const auto first = run(tree(), cache);

    CHECK(touched(first.record, "rates.txt") == std::set<std::string>{"",
        "src/a.txt", "src/b.txt", "src/c.txt"});
    CHECK(touched(first.record, "src/b.side") == std::set<std::string>{"",
        "src/a.txt", "src/b.txt", "src/c.txt"});
    CHECK(touched(first.record, "src/e.txt", true) == std::set<std::string>{
        "", "src/a.txt", "src/b.txt", "src/c.txt"});
    CHECK(touched(first.record, "notes.md").empty());
}

TEST_CASE("a stage reads back as it was written, and a scan table is not "
    "one", "[ops][staged]") {
    types::stage s;
    s.parts = {"a", "b"};
    s.facts.emplace_back("rate", compass::store::types::blob_id{
        compass::store::ops::sha1_of("x")});

    const auto back = ops::decode_stage(ops::encode_stage(s));

    REQUIRE(back);
    CHECK(back->parts == s.parts);
    CHECK(back->facts == s.facts);
    const std::pair<std::string_view, ops::scan_entry> entry{"src/a.txt",
        {}};
    CHECK_FALSE(ops::decode_stage(ops::encode_scan({&entry, 1}, {})));
    CHECK_FALSE(ops::decode_scan(ops::encode_stage(s)));
    CHECK_FALSE(ops::decode_stage(""));
}

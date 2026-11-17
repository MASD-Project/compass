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
#include <filesystem>
#include <fstream>
#include <map>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/codecs/document_index.hpp"
#include "compass.codec/codecs/required_sections.hpp"
#include "compass.codec/io/dependency_file.hpp"
#include "compass.codec/ops/dependencies.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;
namespace ot = compass::org::types;
using Catch::Matchers::ContainsSubstring;

using sources = std::map<std::string, std::string>;

constexpr std::array<std::string_view, 2> stories_and_tasks{"story", "task"};
constexpr std::array<std::string_view, 1> tasks{"task"};
constexpr std::array<types::rule, 1> link_rules{{{"link", ""}}};
constexpr std::array<types::rule, 1> file_rules{{{"long", ""}}};

/**
 * @brief Reports where each id: and file: link of a document leads.
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
        out.add({d.row, {}, {0}, types::severity::warning,
            found ? "links to " + std::string(found->path)
                  : "broken link to " + std::string(target), ""});
    }
}

/**
 * @brief Emits one file listing the path of every task.
 */
void list_tasks(const types::corpus_view& corpus, types::file_table& out) {
    std::string text;
    for (std::size_t i = 0; i < corpus.size(); ++i)
        text += std::string(corpus[i].path) + "\n";
    out.add("tasks.txt", text);
}

/**
 * @brief Reports each file under data/ ending in .txt that is longer than
 * three bytes, on that file, and whether data/flag exists, on
 * src/main.cpp, which it never reads.
 */
void long_files(const types::corpus_view& v, types::diagnostic_table& out) {
    for (const auto p : v.list_files("data/", ".txt")) {
        if (v.read_file(p).value_or("").size() > 3)
            out.add({ot::document_id::none(), {}, {0},
                types::severity::error, "too long", "", std::string(p)});
    }
    if (v.file_exists("data/flag"))
        out.add({ot::document_id::none(), {}, {0}, types::severity::error,
            "flagged", "", "src/main.cpp"});
}

const std::array<types::descriptor, 5> all_codecs{codecs::document_index,
    codecs::required_sections,
    types::descriptor{.name = "follow_links", .version = 1, .summary = "",
        .reads = types::inputs::of_types(stories_and_tasks), .options = {},
        .rules = link_rules, .enabled_by_default = true,
        .run = types::document_function<types::diagnostic_table>{
            follow_links}},
    types::descriptor{.name = "task_list", .version = 1, .summary = "",
        .reads = types::inputs::of_types(tasks), .options = {},
        .rules = {}, .enabled_by_default = true,
        .run = types::corpus_function<types::file_table>{list_tasks}},
    types::descriptor{.name = "long_files", .version = 1, .summary = "",
        .reads = types::inputs::of_types({}), .options = {},
        .rules = file_rules, .enabled_by_default = true,
        .run = types::corpus_function<types::diagnostic_table>{long_files}}};

sources documents() {
    return {
        {"story.org", ":PROPERTIES:\n:ID: S-1\n:END:\n#+type: story\n"
                      "* Goal\n* Status\n"},
        {"task_a.org", "#+type: task\n* Goal\nSee [[id:S-1][the story]] "
                       "and [[id:X-404][later]].\n* Result\n"},
        {"task_b.org", "#+type: task\n* Goal\nSee [[file:extra.org]].\n"},
        {"readme.org", "Plain text.\n"}};
}

sources files() {
    return {{"data/template.txt", "hello\n"}, {"data/short.txt", "x\n"},
        {"src/main.cpp", "int main() {}\n"}};
}

/**
 * @brief A checkout of documents and other files, as a host lists them.
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

    types::run_result run(const types::dependency_record& previous) const {
        const auto r = ops::make_registry(all_codecs, corpus_.types(), {});
        return ops::run(r, view_, corpus_.types(), 2, previous);
    }

    [[nodiscard]] const types::corpus& corpus() const { return view_; }
    [[nodiscard]] const auto& types() const { return corpus_.types(); }

private:
    tests::test_corpus corpus_;
    std::map<std::string, std::string, std::less<>> bytes_;
    types::checkout_files files_;
    types::corpus view_;
};

types::unit_index index_of(std::string_view bytes) {
    const auto header = bytes.substr(0, ops::record_header_size);
    const auto layout = ops::locate_unit_index(header,
        bytes.substr(bytes.size() - ops::record_footer_size), bytes.size());
    return ops::decode_unit_index(header,
        bytes.substr(layout.index_offset, layout.index_size), layout);
}

std::string name_of(const types::dependency_record& r,
    types::record_unit_id u) {
    return std::string(r.codecs[u]) + ":" + std::string(r.documents[u]);
}

/**
 * @brief The units of @p after that ran, by codec and document.
 */
std::set<std::string> ran(const types::run_result& after) {
    std::set<std::string> out;
    for (const auto u : after.fresh)
        out.insert(name_of(after.record, u));
    return out;
}

/**
 * @brief The units of @p before that units_touching names for an edit to
 * @p path, which holds @p ids, by codec and document.
 */
std::set<std::string> touched(const types::dependency_record& before,
    std::string_view path, std::vector<std::string_view> ids = {},
    bool created = false) {
    const auto index = index_of(ops::encode_dependency_record(before));
    std::set<std::string> out;
    const ops::edited_file file{.path = path, .output = path,
        .document = path.ends_with(".org"), .created = created, .ids = ids};
    for (const auto u : ops::units_touching(index, file))
        out.insert(name_of(before, u));
    return out;
}

/**
 * @brief What an edit to one file may change and what units_touching
 * names for it: every unit the edit ran that had run before must be
 * named, and a document unit the edit adds is the hook's to add.
 */
void check_edit(const sources& docs, const sources& plain,
    const std::string& path, const std::string& content,
    std::vector<std::string_view> ids = {}) {
    const checkout before(docs, plain);
    const auto first = before.run({});
    auto edited_docs = docs;
    auto edited_plain = plain;
    (path.ends_with(".org") ? edited_docs : edited_plain)[path] = content;
    const checkout after(edited_docs, edited_plain);
    const auto second = after.run(first.record);

    std::set<std::string> recorded;
    for (std::uint32_t u = 0; u < first.record.size(); ++u)
        recorded.insert(name_of(first.record, {u}));
    const auto named = touched(first.record, path, ids,
        !docs.contains(path) && !plain.contains(path));
    for (const auto& unit : ran(second)) {
        INFO("edit to " << path << " ran " << unit);
        CHECK((!recorded.contains(unit) || named.contains(unit)));
    }
}

class scoped_file final {
public:
    scoped_file() {
        static int counter = 0;
        path_ = std::filesystem::temp_directory_path() /
            ("compass.codec.unit_index." + std::to_string(::getpid()) + "." +
                std::to_string(counter++));
    }
    ~scoped_file() { std::filesystem::remove(path_); }
    scoped_file(const scoped_file&) = delete;
    scoped_file& operator=(const scoped_file&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

    void write(const std::string& bytes) const {
        std::ofstream os(path_, std::ios::binary | std::ios::trunc);
        os << bytes;
    }

private:
    std::filesystem::path path_;
};

}

TEST_CASE("every unit decodes alone through the index to what the whole "
    "record holds", "[ops][unit_index]") {
    const checkout c(documents(), files());
    const auto record = c.run({}).record;
    const auto bytes = ops::encode_dependency_record(record);
    const auto index = index_of(bytes);
    REQUIRE(index.size() == record.size());

    const std::string_view all(bytes);
    types::dependency_record one_by_one;
    types::dependency_record heads;
    for (std::uint32_t u = 0; u < index.size(); ++u) {
        const auto head = all.substr(index.offsets[u],
            index.offsets[u + 1] - index.offsets[u]);
        ops::decode_record_unit(head, index.hashes.values[u], one_by_one,
            all.substr(index.content_offsets[u],
                index.content_offsets[u + 1] - index.content_offsets[u]));
        ops::decode_record_unit(head, index.hashes.values[u], heads);
    }
    CHECK(ops::encode_dependency_record(one_by_one) == bytes);

    REQUIRE(heads.files.size() == record.files.size());
    REQUIRE(heads.files.size() != 0);
    for (std::uint32_t f = 0; f < heads.files.size(); ++f) {
        const types::file_id file{f};
        CHECK(heads.files.paths[file] == record.files.paths[file]);
        CHECK(heads.file_hashes[file] == record.file_hashes[file]);
        CHECK(heads.files.contents[file].empty());
    }
    CHECK(heads.diagnostics.bytes == record.diagnostics.bytes);
    CHECK(heads.input_keys.bytes == record.input_keys.bytes);
}

TEST_CASE("a damaged output fails its own read and the whole record",
    "[ops][unit_index]") {
    const checkout c(documents(), files());
    const auto record = c.run({}).record;
    auto bytes = ops::encode_dependency_record(record);
    auto index = index_of(bytes);
    const auto producer = std::ranges::find_if(std::views::iota(0u,
        static_cast<std::uint32_t>(index.size())), [&](std::uint32_t u) {
            return index.content_offsets[u + 1] > index.content_offsets[u];
        });
    REQUIRE(*producer < index.size());
    const types::record_unit_id unit{*producer};
    const scoped_file f;
    f.write(bytes);
    const auto intact = io::read_recorded_file(f.path(), index, unit, 0);
    REQUIRE(intact);
    CHECK(*intact == record.files.contents[record.file_firsts[unit]]);
    CHECK_FALSE(io::read_recorded_file(f.path(), index, unit, 1));

    bytes[index.content_offsets[unit.value]] ^= 0x01;
    f.write(bytes);
    CHECK_FALSE(io::read_recorded_file(f.path(), index, unit, 0));
    CHECK_THROWS_WITH(ops::decode_dependency_record(bytes),
        ContainsSubstring("blob ID"));
}

TEST_CASE("a damaged unit fails alone and the whole record is rejected",
    "[ops][unit_index]") {
    const checkout c(documents(), files());
    const auto record = c.run({}).record;
    auto bytes = ops::encode_dependency_record(record);
    const auto index = index_of(bytes);
    const auto first = index.offsets[0];
    bytes[first + 2] ^= 0x01;

    types::dependency_record out;
    CHECK_THROWS_WITH(ops::decode_record_unit(std::string_view(bytes).substr(
        first, index.offsets[1] - first), index.hashes.values[0], out),
        ContainsSubstring("unit checksum"));
    CHECK_NOTHROW(ops::decode_record_unit(std::string_view(bytes).substr(
        index.offsets[1], index.offsets[2] - index.offsets[1]),
        index.hashes.values[1], out));
    CHECK_THROWS_AS(ops::decode_dependency_record(bytes), std::runtime_error);
}

TEST_CASE("a damaged index is rejected", "[ops][unit_index]") {
    const checkout c(documents(), files());
    auto bytes = ops::encode_dependency_record(c.run({}).record);
    bytes[bytes.size() - ops::record_footer_size - 3] ^= 0x01;
    CHECK_THROWS_WITH(index_of(bytes), ContainsSubstring("index checksum"));
}

TEST_CASE("the index names the units that read, produce and report on a "
    "path", "[ops][unit_index]") {
    const checkout c(documents(), files());
    const auto record = c.run({}).record;

    CHECK(touched(record, "src/main.cpp").empty());
    CHECK(touched(record, "tasks.txt") ==
        std::set<std::string>{"task_list:"});
    CHECK(touched(record, "data/template.txt") ==
        std::set<std::string>{"long_files:"});
    CHECK(touched(record, "task_b.org") == std::set<std::string>{
        "document_index:", "follow_links:task_b.org",
        "required_sections:task_b.org", "task_list:"});
    auto flagged = files();
    flagged["data/flag"] = "";
    const checkout with_flag(documents(), flagged);
    CHECK(touched(with_flag.run({}).record, "src/main.cpp") ==
        std::set<std::string>{"long_files:"});
}

TEST_CASE("every unit an edit to one file runs again is one the index "
    "names for that file", "[ops][unit_index]") {
    const auto docs = documents();
    const auto plain = files();
    for (const auto& [path, source] : docs)
        check_edit(docs, plain, path, source + "More words.\n");
    for (const auto& [path, source] : plain)
        check_edit(docs, plain, path, source + "more bytes\n");

    check_edit(docs, plain, "story.org",
        ":PROPERTIES:\n:ID: S-2\n:END:\n#+type: story\n* Goal\n");
    check_edit(docs, plain, "readme.org",
        ":PROPERTIES:\n:ID: X-404\n:END:\nPlain text.\n", {"X-404"});
    check_edit(docs, plain, "readme.org", "#+type: task\n* Goal\n");
    check_edit(docs, plain, "extra.org", "#+type: story\n* Goal\n");
    check_edit(docs, plain, "data/new.txt", "a new file\n");
    check_edit(docs, plain, "data/flag", "");
}

TEST_CASE("an ID the edit adds names the units that looked it up",
    "[ops][unit_index]") {
    const checkout c(documents(), files());
    const auto record = c.run({}).record;
    CHECK(touched(record, "readme.org", {"X-404"}).contains(
        "follow_links:task_a.org"));
    CHECK_FALSE(touched(record, "readme.org").contains(
        "follow_links:task_a.org"));
}

TEST_CASE("units are read alone from the record file, and a file "
    "replaced since the index was read is a problem", "[io][unit_index]") {
    const checkout c(documents(), files());
    const auto record = c.run({}).record;
    const scoped_file f;
    f.write(ops::encode_dependency_record(record));

    const auto loaded = io::read_unit_index(f.path());
    REQUIRE(loaded.problem.empty());
    CHECK_FALSE(loaded.missing);
    const std::array<types::record_unit_id, 2> wanted{
        types::record_unit_id{3}, types::record_unit_id{1}};
    const auto units = io::read_record_units(f.path(), loaded.index, wanted);
    REQUIRE(units.problem.empty());
    REQUIRE(units.record.size() == 2);
    CHECK(name_of(units.record, {0}) == name_of(record, {3}));
    CHECK(name_of(units.record, {1}) == name_of(record, {1}));

    auto other = documents();
    other["story.org"] += "Changed.\n";
    const checkout changed(other, files());
    f.write(ops::encode_dependency_record(changed.run({}).record));
    const std::array<types::record_unit_id, 1> story{
        types::record_unit_id{0}};
    CHECK_FALSE(io::read_record_units(f.path(), loaded.index,
        story).problem.empty());
}

TEST_CASE("a missing record file has a missing index and a file of "
    "another version a problem", "[io][unit_index]") {
    const scoped_file f;
    const auto missing = io::read_unit_index(f.path());
    CHECK(missing.missing);
    CHECK(missing.problem.empty());

    f.write(std::string("CMPSDEP\n\x04\0\0\0", 12) + std::string(64, '\0'));
    const auto old = io::read_unit_index(f.path());
    CHECK_FALSE(old.missing);
    CHECK_THAT(old.problem, ContainsSubstring("version 4"));
}

TEST_CASE("a run with a filter takes only the units it allows, with the "
    "outputs a full run gives them", "[ops][run]") {
    const checkout c(documents(), files());
    const auto r = ops::make_registry(all_codecs, c.types(), {});
    const auto full = c.run({});
    const auto only = [&](types::codec_id codec, std::string_view document) {
        const auto name = r.codecs[codec.value].name;
        return (name == "follow_links" && document == "task_a.org") ||
            name == "task_list";
    };

    const auto fresh = ops::run(r, c.corpus(), c.types(), 2, {}, {}, only);
    REQUIRE(fresh.record.size() == 2);
    CHECK(name_of(fresh.record, {0}) == "follow_links:task_a.org");
    CHECK(name_of(fresh.record, {1}) == "task_list:");
    CHECK(fresh.ran == std::vector<std::uint64_t>{0, 0, 1, 1, 0});
    CHECK(fresh.diagnostics[2].size() == 2);
    CHECK(fresh.files[3].contents[{0}] == full.files[3].contents[{0}]);

    const auto reused = ops::run(r, c.corpus(), c.types(), 2, full.record, {},
        only);
    CHECK(reused.skipped == std::vector<std::uint64_t>{0, 0, 1, 1, 0});
    CHECK(reused.ran == std::vector<std::uint64_t>{0, 0, 0, 0, 0});
}

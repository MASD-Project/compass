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
#include "compass.codec/io/output_sink.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/ops/outputs.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.platform/filesystem/file_status.hpp"
#include "compass.platform/filesystem/new_file.hpp"
#include "compass.store/io/git_files.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;
using Catch::Matchers::ContainsSubstring;

struct row {
    std::string codec;
    std::string path;
    std::string content;
};

/**
 * @brief The outputs of @p rows, with every codec they name complete,
 * plus the codecs of @p also.
 */
types::output_set outputs(const std::vector<row>& rows,
    const std::vector<std::string>& also = {}) {
    types::output_set s;
    for (const auto& r : rows) {
        s.add(r.codec, r.path, r.content,
            compass::store::ops::blob_id_of(r.content));
        if (std::ranges::find(s.complete, r.codec) == s.complete.end())
            s.complete.push_back(r.codec);
    }
    for (const auto& c : also)
        s.complete.push_back(c);
    return s;
}

std::string read(const std::filesystem::path& p) {
    std::ifstream is(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(is), {}};
}

void write(const std::filesystem::path& p, const std::string& bytes) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream os(p, std::ios::binary | std::ios::trunc);
    os << bytes;
}

/**
 * @brief Everything a write would change about a file: its inode, its
 * modification and change times to the nanosecond, and its size.
 */
using stamp = std::tuple<std::uint64_t, std::int64_t, std::uint32_t,
    std::int64_t, std::uint32_t, std::uint64_t>;

stamp stamp_of(const std::filesystem::path& p) {
    const auto st = compass::platform::filesystem::regular_file_status(p);
    REQUIRE(st);
    return {st->inode, st->modified.seconds, st->modified.nanoseconds,
        st->changed.seconds, st->changed.nanoseconds, st->size};
}

/**
 * @brief An output root under a fresh temporary directory, and the
 * record of what the sink wrote there, carried from run to run.
 */
class sink final {
    compass::testing::filesystem::scoped_directory dir_{
        "compass.codec.sink.tests"};

public:
    sink() {
        std::filesystem::create_directories(root());
    }

    [[nodiscard]] std::filesystem::path root() const {
        return dir_.path() / "out";
    }
    [[nodiscard]] const std::filesystem::path& dir() const {
        return dir_.path();
    }

    io::sink_result run(const types::output_set& s) {
        auto r = io::write_outputs(root(), s, record);
        if (r.problems.empty())
            record = r.record;
        return r;
    }

    /**
     * @brief Moves the record's written time past every file it names, as
     * if the next run came a second later, so stat data is trusted.
     */
    void age() {
        for (const auto& s : record.stats)
            record.written.sec = std::max(record.written.sec, s.mtime.sec + 1);
    }

    types::written_outputs record;
};

/**
 * @brief Every regular file under @p dir with its bytes and its stamp.
 */
std::map<std::string, std::pair<std::string, stamp>> snapshot(
    const std::filesystem::path& dir) {
    std::map<std::string, std::pair<std::string, stamp>> out;
    for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
        if (e.is_regular_file())
            out[e.path().generic_string()] = {read(e.path()),
                stamp_of(e.path())};
    }
    return out;
}

/**
 * @brief Writes page.org from the task's title, and nothing when the
 * task has no title, which it reports as a refusal.
 */
void page(const types::corpus_view& v, types::file_output& out) {
    const auto& d = v[0];
    const auto at = d.source.find("#+title: ");
    if (at == std::string_view::npos) {
        out.refusals.add({d.row, {}, {0}, types::severity::error,
            "page.org is not written: the task has no title", ""});
        return;
    }
    out.files.add("page.org", d.source.substr(at));
}

constexpr std::array<types::rule, 1> page_rules{{{"refused", ""}}};

const types::descriptor page_codec{.name = "page", .version = 1,
    .summary = "", .reads = types::inputs::every_document(), .options = {},
    .rules = page_rules, .enabled_by_default = true,
    .run = types::corpus_function<types::file_output>{page}};

/**
 * @brief The outputs of the page codec run over one document, @p source.
 */
types::output_set page_outputs(std::string_view source) {
    tests::test_corpus t;
    t.add("task.org", source);
    const std::array codecs{page_codec};
    const auto r = ops::make_registry(codecs, t.types(), {});
    return ops::outputs_of(r, ops::run(r, t.corpus(), t.types(), 1));
}

const std::vector<row> two = {
    {"gen", "index.tsv", "path\ttitle\n"},
    {"gen", "deep/dir/model.hpp", "struct model {};\n"}};

}

TEST_CASE("the written-outputs record lives under compass in the git "
    "directory", "[io][output_sink]") {
    CHECK(io::written_outputs_path("/r/.git") == "/r/.git/compass/outputs");
}

TEST_CASE("the first write creates every output under the root",
    "[io][output_sink]") {
    sink s;
    const auto r = s.run(outputs(two));
    CHECK(r.problems.empty());
    CHECK(r.written == 2);
    CHECK(r.unchanged + r.restored + r.removed + r.kept == 0);
    CHECK(read(s.root() / "index.tsv") == "path\ttitle\n");
    CHECK(read(s.root() / "deep/dir/model.hpp") == "struct model {};\n");
    CHECK(s.record.paths ==
        std::vector<std::string>{"deep/dir/model.hpp", "index.tsv"});
    CHECK(s.record.root == s.root().generic_string());
}

TEST_CASE("a run with nothing changed writes nothing and keeps every "
    "modification time", "[io][output_sink]") {
    sink s;
    s.run(outputs(two));
    const auto index = stamp_of(s.root() / "index.tsv");
    const auto model = stamp_of(s.root() / "deep/dir/model.hpp");

    // Straight after the first write every file is racily clean, so this
    // run compares contents; the aged run trusts stat data.
    for (const bool aged : {false, true}) {
        if (aged)
            s.age();
        const auto r = s.run(outputs(two));
        CHECK(r.unchanged == 2);
        CHECK(r.written + r.restored + r.removed + r.kept == 0);
        CHECK(stamp_of(s.root() / "index.tsv") == index);
        CHECK(stamp_of(s.root() / "deep/dir/model.hpp") == model);
    }
}

TEST_CASE("a changed output is rewritten and the others are untouched",
    "[io][output_sink]") {
    sink s;
    s.run(outputs(two));
    s.age();
    const auto model = stamp_of(s.root() / "deep/dir/model.hpp");

    const auto r = s.run(outputs({{"gen", "index.tsv", "path\ttitle\nA\n"},
        two[1]}));
    CHECK(r.written == 1);
    CHECK(r.unchanged == 1);
    CHECK(read(s.root() / "index.tsv") == "path\ttitle\nA\n");
    CHECK(stamp_of(s.root() / "deep/dir/model.hpp") == model);
}

TEST_CASE("a hand-edited output is restored and a deleted one recreated",
    "[io][output_sink]") {
    for (const bool aged : {false, true}) {
        sink s;
        s.run(outputs(two));
        if (aged)
            s.age();
        // Same size, so only the stat times and the content tell. An aged
        // record stands for an edit made later, so its time moves on.
        const auto edited = s.root() / "index.tsv";
        write(edited, "PATH\tTITLE\n");
        if (aged)
            std::filesystem::last_write_time(edited,
                std::filesystem::last_write_time(edited) +
                    std::chrono::seconds(2));
        std::filesystem::remove(s.root() / "deep/dir/model.hpp");

        const auto r = s.run(outputs(two));
        CHECK(r.restored == 2);
        CHECK(r.written + r.unchanged == 0);
        CHECK(read(s.root() / "index.tsv") == "path\ttitle\n");
        CHECK(read(s.root() / "deep/dir/model.hpp") == "struct model {};\n");
    }
}

TEST_CASE("an output no longer produced is removed, and nothing compass "
    "did not write is", "[io][output_sink]") {
    sink s;
    s.run(outputs({two[0], two[1], {"other", "other.txt", "o\n"},
        {"edited", "edited.txt", "e\n"}}));
    write(s.root() / "notes.txt", "mine\n");
    write(s.root() / "deep/notes.txt", "mine too\n");
    write(s.root() / "edited.txt", "hand made\n");

    // "other" is not complete, as a disabled codec is not.
    const auto r = s.run(outputs({two[0]}, {"edited"}));
    CHECK(r.removed == 1);
    CHECK(r.kept == 1);
    CHECK(r.unchanged == 1);
    CHECK_FALSE(std::filesystem::exists(s.root() / "deep/dir"));
    CHECK(read(s.root() / "deep/notes.txt") == "mine too\n");
    CHECK(read(s.root() / "notes.txt") == "mine\n");
    CHECK(read(s.root() / "other.txt") == "o\n");
    CHECK(read(s.root() / "edited.txt") == "hand made\n");
    CHECK(s.record.paths ==
        std::vector<std::string>{"index.tsv", "other.txt"});

    const auto again = s.run(outputs({two[0]}, {"edited"}));
    CHECK(again.removed + again.kept == 0);
    CHECK(read(s.root() / "edited.txt") == "hand made\n");
}

TEST_CASE("a lost or damaged record compares by content and touches no "
    "unchanged file", "[io][output_sink]") {
    sink s;
    s.run(outputs({two[0], two[1], {"gen", "gone.txt", "g\n"}}));
    const auto path = io::written_outputs_path(s.dir() / "git");
    REQUIRE(io::write_written_outputs(path, s.record));
    const auto index = stamp_of(s.root() / "index.tsv");
    const auto model = stamp_of(s.root() / "deep/dir/model.hpp");

    auto bytes = read(path);
    bytes[bytes.size() / 2] ^= 0x01;
    write(path, bytes);
    const auto loaded = io::read_written_outputs(path);
    CHECK_THAT(loaded.problem, ContainsSubstring("checksum"));
    CHECK(loaded.record.size() == 0);

    for (const auto& previous : {loaded.record, types::written_outputs{}}) {
        s.record = previous;
        const auto r = s.run(outputs(two));
        CHECK(r.unchanged == 2);
        CHECK(r.written + r.restored + r.removed + r.kept == 0);
        CHECK(stamp_of(s.root() / "index.tsv") == index);
        CHECK(stamp_of(s.root() / "deep/dir/model.hpp") == model);
        CHECK(std::filesystem::exists(s.root() / "gone.txt"));
    }
}

TEST_CASE("a path escaping the root is rejected and nothing is written",
    "[io][output_sink]") {
    for (const auto* bad : {"../escape.txt", "deep/../../escape.txt"}) {
        sink s;
        const auto r = s.run(outputs({two[0], {"gen", bad, "x\n"}}));
        REQUIRE(r.problems.size() == 1);
        CHECK_THAT(r.problems[0], ContainsSubstring(bad));
        CHECK(std::filesystem::is_empty(s.root()));
        CHECK_FALSE(std::filesystem::exists(s.dir() / "escape.txt"));
        CHECK(s.record.size() == 0);
    }
    sink s;
    const auto absolute = (s.dir() / "abs.txt").generic_string();
    CHECK(s.run(outputs({{"gen", absolute, "x\n"}})).problems.size() == 1);
    CHECK_FALSE(std::filesystem::exists(absolute));
}

TEST_CASE("a path through a symbolic link is rejected, and a removal "
    "through one leaves the target alone", "[io][output_sink]") {
    sink s;
    const auto outside = s.dir() / "outside";
    std::filesystem::create_directories(outside);
    std::filesystem::create_directory_symlink(outside, s.root() / "link");

    const auto r = s.run(outputs({two[0], {"gen", "link/a.txt", "x\n"}}));
    REQUIRE(r.problems.size() == 1);
    CHECK_THAT(r.problems[0], ContainsSubstring("link/a.txt"));
    CHECK_FALSE(std::filesystem::exists(outside / "a.txt"));
    CHECK_FALSE(std::filesystem::exists(s.root() / "index.tsv"));

    std::filesystem::remove(s.root() / "link");
    s.run(outputs({two[0], {"gen", "link/b.txt", "b\n"}}));
    REQUIRE(read(s.root() / "link/b.txt") == "b\n");
    std::filesystem::remove_all(s.root() / "link");
    write(outside / "b.txt", "b\n");
    std::filesystem::create_directory_symlink(outside, s.root() / "link");

    const auto removal = s.run(outputs({two[0]}));
    CHECK(removal.removed == 0);
    CHECK(removal.kept == 1);
    CHECK(read(outside / "b.txt") == "b\n");
}

TEST_CASE("two codecs producing one path is rejected and nothing is "
    "written", "[io][output_sink]") {
    sink s;
    const auto r = s.run(outputs({two[0], {"a", "same.txt", "1\n"},
        {"b", "same.txt", "2\n"}}));
    REQUIRE(r.problems.size() == 1);
    CHECK(r.problems[0] == "a and b both produced \"same.txt\"");
    CHECK(std::filesystem::is_empty(s.root()));
}

TEST_CASE("a record of another root is ignored and removes nothing",
    "[io][output_sink]") {
    sink s;
    s.run(outputs(two));
    s.record.root = "/elsewhere";
    const auto r = s.run(outputs({two[0]}));
    CHECK(r.unchanged == 1);
    CHECK(r.removed == 0);
    CHECK(std::filesystem::exists(s.root() / "deep/dir/model.hpp"));
}

TEST_CASE("the written-outputs file is rewritten only when it changes",
    "[io][output_sink]") {
    sink s;
    s.run(outputs(two));
    const auto path = io::written_outputs_path(s.dir() / "git");
    CHECK(io::write_written_outputs(path, s.record));
    const auto before = stamp_of(path);
    CHECK_FALSE(io::write_written_outputs(path, s.record));
    CHECK(stamp_of(path) == before);
    CHECK(read(path) == ops::encode_written_outputs(s.record));

    s.age();
    s.run(outputs(two));
    const auto aged = s.record;
    s.run(outputs(two));
    CHECK(s.record.written == aged.written);
}

TEST_CASE("a plan touches nothing, and applying it does what it says",
    "[io][output_sink]") {
    sink s;
    s.run(outputs({two[0], two[1], {"gen", "gone.txt", "g\n"},
        {"edited", "edited.txt", "e\n"}}));
    write(s.root() / "edited.txt", "hand made\n");
    std::filesystem::remove(s.root() / "deep/dir/model.hpp");
    const auto next = outputs({{"gen", "index.tsv", "path\ttitle\nA\n"},
        two[1], {"gen", "new.txt", "n\n"}}, {"edited"});
    const auto before = snapshot(s.dir());

    const auto plan = io::plan_outputs(s.root(), next, s.record);
    const auto diff = io::diff_outputs(plan);

    CHECK(snapshot(s.dir()) == before);
    REQUIRE(plan.problems.empty());
    CHECK(plan.paths == std::vector<std::string_view>{"index.tsv",
        "deep/dir/model.hpp", "new.txt", "edited.txt", "gone.txt"});
    using enum io::output_action;
    CHECK(plan.actions == std::vector<io::output_action>{write, restore,
        write, keep, remove});
    CHECK(diff ==
        "--- a/index.tsv\n+++ b/index.tsv\n@@ -1 +1,2 @@\n path\ttitle\n+A\n"
        "--- /dev/null\n+++ b/deep/dir/model.hpp\n@@ -0,0 +1 @@\n"
        "+struct model {};\n"
        "--- /dev/null\n+++ b/new.txt\n@@ -0,0 +1 @@\n+n\n"
        "--- a/gone.txt\n+++ /dev/null\n@@ -1 +0,0 @@\n-g\n");

    const auto planned = io::summarise(plan);
    const auto r = io::apply_outputs(plan);
    CHECK(std::tie(r.written, r.restored, r.unchanged, r.removed, r.kept) ==
        std::tie(planned.written, planned.restored, planned.unchanged,
            planned.removed, planned.kept));
    CHECK(std::tie(r.written, r.restored, r.removed, r.kept) ==
        std::tuple{2u, 1u, 1u, 1u});
    CHECK(r.record.paths == plan.record.paths);
    CHECK(r.record.hashes == plan.record.hashes);
    CHECK(r.record.written == plan.record.written);
    CHECK(read(s.root() / "index.tsv") == "path\ttitle\nA\n");
    CHECK(read(s.root() / "deep/dir/model.hpp") == "struct model {};\n");
    CHECK(read(s.root() / "new.txt") == "n\n");
    CHECK(read(s.root() / "edited.txt") == "hand made\n");
    CHECK_FALSE(std::filesystem::exists(s.root() / "gone.txt"));
    for (std::size_t i = 0; i < r.record.size(); ++i)
        CHECK(r.record.stats[i] ==
            *compass::store::io::stat_of(s.root() / r.record.paths[i]));
}

TEST_CASE("applying a plan to write a file that already holds the content "
    "leaves the file untouched", "[io][output_sink]") {
    sink s;
    s.run(outputs(two));
    s.age();
    const auto next = outputs({{"gen", "index.tsv", "path\ttitle\nA\n"},
        two[1]});
    const auto plan = io::plan_outputs(s.root(), next, s.record);
    REQUIRE(plan.actions.front() == io::output_action::write);
    write(s.root() / "index.tsv", "path\ttitle\nA\n");
    const auto before = snapshot(s.dir());

    const auto r = io::apply_outputs(plan);

    CHECK(r.problems.empty());
    CHECK(snapshot(s.dir()) == before);
    CHECK(r.record.stats[1] ==
        *compass::store::io::stat_of(s.root() / "index.tsv"));
}

TEST_CASE("a write leaves no temporary, and clears one a killed run left "
    "beside its file", "[io][output_sink]") {
    sink s;
    s.run(outputs(two));
    s.age();
    write(s.root() / ("index.tsv" +
        std::string(compass::platform::filesystem::update_suffix)), "hal");

    s.run(outputs({{"gen", "index.tsv", "path\ttitle\nA\n"}, two[1]}));

    std::vector<std::string> files;
    for (const auto& e : std::filesystem::recursive_directory_iterator(
             s.root()))
        if (e.is_regular_file())
            files.push_back(std::filesystem::relative(e.path(), s.root())
                .generic_string());
    std::ranges::sort(files);
    CHECK(files == std::vector<std::string>{"deep/dir/model.hpp",
        "index.tsv"});
}

TEST_CASE("a plan with nothing changed has no diff and keeps the record",
    "[io][output_sink]") {
    sink s;
    s.run(outputs(two));
    s.age();

    const auto plan = io::plan_outputs(s.root(), outputs(two), s.record);

    CHECK(plan.actions == std::vector<io::output_action>(2,
        io::output_action::unchanged));
    CHECK(io::diff_outputs(plan).empty());
    CHECK(ops::encode_written_outputs(plan.record) ==
        ops::encode_written_outputs(s.record));
}

TEST_CASE("a generator that refuses after a good run keeps the file it "
    "wrote", "[io][output_sink]") {
    sink s;
    s.run(page_outputs("#+title: Task: a\n"));
    REQUIRE(read(s.root() / "page.org") == "#+title: Task: a\n");

    const auto r = s.run(page_outputs("No title.\n"));
    CHECK(r.removed == 0);
    CHECK(read(s.root() / "page.org") == "#+title: Task: a\n");
}

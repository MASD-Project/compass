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
#include "compass.codec/io/format_cache.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/outputs.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.codec/types/corpus_view.hpp"

namespace {

using namespace compass::codec;

void write_file(const std::filesystem::path& p, std::string_view text) {
    std::ofstream(p, std::ios::binary) << text;
}

/**
 * @brief A store and a fake formatter, a shell script that rewrites each
 * file it is given as [style:name]content, logs a line per process to the
 * log file and reports the version held in a file of its own.
 */
class fixture final {
public:
    fixture() {
        static int counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("compass.codec.format." + std::to_string(::getpid()) + "." +
                std::to_string(counter++));
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_ / "git");
        write_file(style(), "upper");
        write_file(root_ / "version", "fake 1\n");
        set_script("");
    }
    ~fixture() { std::filesystem::remove_all(root_); }
    fixture(const fixture&) = delete;
    fixture& operator=(const fixture&) = delete;

    /**
     * @brief Rewrites the script with @p extra spliced in before the files
     * are rewritten, and gives it a new modification time.
     */
    void set_script(std::string_view extra) {
        const auto script = program();
        write_file(script, "#!/bin/sh\n"
            "if [ \"$1\" = --version ]; then cat '" +
                (root_ / "version").string() + "'; exit 0; fi\n"
            "echo run >> '" + log().string() + "'\n" + std::string(extra) +
            "style=$(cat \"${1#--style=}\"); shift\n"
            "for f in \"$@\"; do\n"
            "  body=$(cat \"$f\"); printf '[%s:%s]%s' \"$style\" "
            "\"$(basename \"$f\")\" \"$body\" > \"$f\"\n"
            "done\n");
        std::filesystem::permissions(script,
            std::filesystem::perms::owner_all);
        const auto now = std::filesystem::file_time_type::clock::now();
        std::filesystem::last_write_time(script, now +
            std::chrono::seconds(++touches_));
    }

    types::formatter formatter() const {
        return {.name = "fake", .program = program(),
            .extensions = {".hpp"}, .arguments = {"--style={style}"},
            .passes = 1, .style = style()};
    }

    std::filesystem::path git() const { return root_ / "git"; }
    std::filesystem::path style() const { return root_ / "style"; }
    std::filesystem::path program() const { return root_ / "fake-format"; }
    std::filesystem::path log() const { return root_ / "log"; }
    std::filesystem::path version() const { return root_ / "version"; }

    std::size_t processes() const {
        std::ifstream is(log());
        std::size_t n = 0;
        for (std::string line; std::getline(is, line);)
            ++n;
        return n;
    }

private:
    std::filesystem::path root_;
    int touches_ = 0;
};

types::file_table table_of(
    std::initializer_list<std::pair<std::string_view, std::string_view>> rows) {
    types::file_table t;
    for (const auto& [path, content] : rows)
        t.add(path, content);
    return t;
}

std::string text(const types::file_table& t, std::uint32_t row) {
    return std::string(t.contents[types::file_id{row}]);
}

}

TEST_CASE("format_files formats each file and keeps rows in order",
    "[format_cache]") {
    const fixture f;
    const std::array formatters{f.formatter()};
    const auto raw = table_of({{"a/x.hpp", "one"}, {"y.sql", "two"},
        {"b/z.hpp", "three"}});

    const auto r = io::format_files(f.git(), formatters, raw, 2);

    REQUIRE(r.files.size() == 3);
    CHECK(r.files.paths[types::file_id{1}] == "y.sql");
    CHECK(text(r.files, 0) == "[upper:x.hpp]one");
    CHECK(text(r.files, 1) == "two");
    CHECK(text(r.files, 2) == "[upper:z.hpp]three");
    CHECK(r.stats.formatted == 2);
    CHECK(r.stats.unformatted == 1);
    CHECK(r.stats.hits == 0);
}

TEST_CASE("format_files runs no program when every formatted form is stored",
    "[format_cache]") {
    const fixture f;
    const std::array formatters{f.formatter()};
    const auto raw = table_of({{"a/x.hpp", "one"}, {"b/z.hpp", "three"}});
    const auto cold = io::format_files(f.git(), formatters, raw, 2);
    const auto before = f.processes();

    const auto warm = io::format_files(f.git(), formatters, raw, 2);

    CHECK(warm.stats.runs == 0);
    CHECK(warm.stats.hits == 2);
    CHECK(warm.stats.formatted == 0);
    CHECK(f.processes() == before);
    CHECK(warm.files.contents.bytes == cold.files.contents.bytes);
}

TEST_CASE("format_files formats only the files whose raw bytes changed",
    "[format_cache]") {
    const fixture f;
    const std::array formatters{f.formatter()};
    (void)io::format_files(f.git(), formatters,
        table_of({{"x.hpp", "one"}, {"z.hpp", "three"}}), 2);

    const auto r = io::format_files(f.git(), formatters,
        table_of({{"x.hpp", "one"}, {"z.hpp", "four"}}), 2);

    CHECK(r.stats.hits == 1);
    CHECK(r.stats.formatted == 1);
    CHECK(text(r.files, 1) == "[upper:z.hpp]four");
}

TEST_CASE("format_files reformats a stored file when the style file changes",
    "[format_cache]") {
    const fixture f;
    const std::array formatters{f.formatter()};
    const auto raw = table_of({{"x.hpp", "one"}});
    (void)io::format_files(f.git(), formatters, raw, 1);
    write_file(f.style(), "lower");

    const auto r = io::format_files(f.git(), formatters, raw, 1);

    CHECK(r.stats.hits == 0);
    CHECK(r.stats.formatted == 1);
    CHECK(text(r.files, 0) == "[lower:x.hpp]one");
}

TEST_CASE("format_files reformats when a replaced program reports a new "
    "version", "[format_cache]") {
    fixture f;
    const std::array formatters{f.formatter()};
    const auto raw = table_of({{"x.hpp", "one"}});
    (void)io::format_files(f.git(), formatters, raw, 1);
    write_file(f.version(), "fake 2\n");
    f.set_script("");

    const auto r = io::format_files(f.git(), formatters, raw, 1);

    CHECK(r.stats.hits == 0);
    CHECK(r.stats.formatted == 1);
}

TEST_CASE("format_files keeps the stored version of an unchanged program",
    "[format_cache]") {
    const fixture f;
    const std::array formatters{f.formatter()};
    const auto raw = table_of({{"x.hpp", "one"}});
    (void)io::format_files(f.git(), formatters, raw, 1);
    write_file(f.version(), "fake 2\n");

    const auto r = io::format_files(f.git(), formatters, raw, 1);

    CHECK(r.stats.runs == 0);
    CHECK(r.stats.hits == 1);
}

TEST_CASE("format_files runs each pass over a batch in one process",
    "[format_cache]") {
    const fixture f;
    auto two = f.formatter();
    two.passes = 2;
    const std::array formatters{two};
    types::file_table raw;
    const auto count = io::format_batch + 10;
    for (std::size_t i = 0; i < count; ++i)
        raw.add("d" + std::to_string(i % 7) + "/f" + std::to_string(i) +
            ".hpp", "x");

    const auto r = io::format_files(f.git(), formatters, raw, 2);

    CHECK(f.processes() == 4);
    CHECK(r.stats.runs == 5);
    CHECK(text(r.files, 3) == "[upper:f3.hpp][upper:f3.hpp]x");
}

TEST_CASE("format_files formats each row of a path its own bytes give",
    "[format_cache]") {
    const fixture f;
    const std::array formatters{f.formatter()};
    const auto raw = table_of({{"a/x.hpp", "one"}, {"a/x.hpp", "two"}});

    const auto r = io::format_files(f.git(), formatters, raw, 1);

    CHECK(text(r.files, 0) == "[upper:x.hpp]one");
    CHECK(text(r.files, 1) == "[upper:x.hpp]two");
    const auto again = io::format_files(f.git(), formatters,
        table_of({{"a/x.hpp", "one"}}), 1);
    CHECK(again.stats.hits == 1);
    CHECK(text(again.files, 0) == "[upper:x.hpp]one");
}

TEST_CASE("format_files formats rows of one path and bytes once",
    "[format_cache]") {
    const fixture f;
    const std::array formatters{f.formatter()};
    const auto raw = table_of({{"a/x.hpp", "one"}, {"a/x.hpp", "one"}});

    const auto r = io::format_files(f.git(), formatters, raw, 1);

    CHECK(r.stats.formatted == 2);
    CHECK(f.processes() == 1);
    CHECK(text(r.files, 0) == "[upper:x.hpp]one");
    CHECK(text(r.files, 1) == "[upper:x.hpp]one");
}

TEST_CASE("format_files does not reuse the form of the same bytes at another "
    "path", "[format_cache]") {
    const fixture f;
    const std::array formatters{f.formatter()};
    (void)io::format_files(f.git(), formatters,
        table_of({{"a/x.hpp", "one"}}), 1);

    const auto r = io::format_files(f.git(), formatters,
        table_of({{"a/y.hpp", "one"}}), 1);

    CHECK(r.stats.hits == 0);
    CHECK(text(r.files, 0) == "[upper:y.hpp]one");
}

TEST_CASE("format_files runs nothing when no file selects a formatter",
    "[format_cache]") {
    const fixture f;
    const std::array formatters{f.formatter()};
    const auto r = io::format_files(f.git(), formatters,
        table_of({{"y.sql", "two"}}), 1);
    CHECK(r.stats.runs == 0);
    CHECK(text(r.files, 0) == "two");
}

TEST_CASE("format_files throws and stores nothing when the formatter fails",
    "[format_cache]") {
    fixture f;
    f.set_script("exit 3\n");
    const std::array formatters{f.formatter()};
    const auto raw = table_of({{"x.hpp", "one"}});
    CHECK_THROWS_AS(io::format_files(f.git(), formatters, raw, 1),
        std::runtime_error);

    f.set_script("");
    const auto r = io::format_files(f.git(), formatters, raw, 1);
    CHECK(r.stats.formatted == 1);
    CHECK(text(r.files, 0) == "[upper:x.hpp]one");
}

TEST_CASE("find_program searches PATH and accepts a path with a slash",
    "[format_cache]") {
    const fixture f;
    CHECK(io::find_program("sh").has_value());
    CHECK(io::find_program(f.program().string()) == f.program());
    CHECK_FALSE(io::find_program("compass-no-such-program").has_value());
}

namespace {

void no_files(const types::corpus_view&, types::file_table&) {}

std::vector<types::formatter> fake_formatters(const types::option_values& o,
    const std::filesystem::path& checkout) {
    return {{.name = "fake", .program = checkout / o["program"],
        .extensions = {".hpp"}, .arguments = {"--style={style}"},
        .passes = 1, .style = checkout / o["style"]}};
}

constexpr std::array<types::option, 2> fake_options{{
    {"program", "fake-format", ""}, {"style", "style", ""}}};

types::descriptor files_codec(std::string_view name,
    types::formatters_function formatters) {
    return {.name = name, .version = 1, .summary = "",
        .reads = types::inputs::every_document(), .options = fake_options,
        .rules = {}, .enabled_by_default = true,
        .run = types::corpus_function<types::file_table>{no_files},
        .formatters = formatters};
}

}

TEST_CASE("format_outputs formats the files of the enabled codecs that "
    "have formatters, with the formatters their options give",
    "[format_cache]") {
    const fixture f;
    const auto checkout = f.git().parent_path();
    write_file(checkout / "style2", "lower");
    const std::array codecs{files_codec("formatted", fake_formatters),
        files_codec("plain", nullptr), files_codec("off", fake_formatters)};
    types::configuration settings;
    settings.set("formatted", "style", "style2");
    settings.set("off", types::enabled_key, "false");
    const auto r = ops::make_registry(codecs, {}, settings);
    REQUIRE(r.problems.empty());
    types::run_result run;
    for (std::size_t c = 0; c < codecs.size(); ++c)
        run.files.push_back(table_of({{"x.hpp", "one"}, {"y.sql", "two"}}));

    const auto stats = io::format_outputs(r, run, checkout, f.git(), 2);

    CHECK(text(run.files[0], 0) == "[lower:x.hpp]one");
    CHECK(text(run.files[0], 1) == "two");
    CHECK(text(run.files[1], 0) == "one");
    CHECK(text(run.files[2], 0) == "one");
    REQUIRE(stats.size() == 3);
    REQUIRE(stats[0].has_value());
    CHECK(stats[0]->formatted == 1);
    CHECK(stats[0]->unformatted == 1);
    CHECK_FALSE(stats[1].has_value());
    CHECK_FALSE(stats[2].has_value());

    // The output set takes the formatted codec's files, not its record's.
    const auto set = ops::outputs_of(r, run);
    REQUIRE(set.size() == 2);
    CHECK(set.files.contents[types::file_id{0}] == "[lower:x.hpp]one");
    CHECK(set.hashes[types::file_id{0}] ==
        compass::store::ops::blob_id_of("[lower:x.hpp]one"));
    CHECK(set.codecs[types::file_id{1}] == "formatted");
}

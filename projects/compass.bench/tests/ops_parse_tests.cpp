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
#include "compass.bench/ops/parse.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.bench/io/report.hpp"
#include "compass.bench/ops/listing.hpp"
#include "compass.bench/ops/parse_stage.hpp"
#include "compass.platform/process/run_program.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"

namespace {

using namespace compass::bench;
using types::language;

/**
 * @brief Counts by visiting every node, with no pruning: the oracle.
 */
std::uint32_t count_every_node(TSNode node) {
    std::uint32_t count = ts_node_is_error(node) || ts_node_is_missing(node);
    for (std::uint32_t i = 0; i < ts_node_child_count(node); ++i)
        count += count_every_node(ts_node_child(node, i));
    return count;
}

struct counts {
    std::uint32_t pruned = 0;
    std::uint32_t oracle = 0;
};

counts parse(language l, std::string_view text) {
    TSParser* parser = ts_parser_new();
    REQUIRE(ts_parser_set_language(parser, ops::grammar_of(l)));
    TSTree* tree = ts_parser_parse_string(parser, nullptr, text.data(),
        static_cast<std::uint32_t>(text.size()));
    REQUIRE(tree != nullptr);
    const auto root = ts_tree_root_node(tree);
    const counts r{ops::count_error_nodes(root), count_every_node(root)};
    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return r;
}

class scoped_git_repo final {
    compass::testing::filesystem::scoped_directory dir_{"compass.bench.parse"};

public:
    void write(const std::string& relative, std::string_view content) {
        const auto path = dir_.path() / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream os(path, std::ios::binary);
        os.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    void track_all() {
        const auto command = "git -C '" + dir_.path().generic_string() +
            "' init -q && git -C '" + dir_.path().generic_string() + "' add -A";
        REQUIRE(compass::platform::process::run_program(
            {{"sh", "-c", command}, {}, {}}).exit_code == 0);
    }

    const std::filesystem::path& root() const { return dir_.path(); }
};

types::file_table list(const std::filesystem::path& root) {
    const auto raw = ops::git_ls_files(root);
    return ops::build_file_table(root, ops::parse_listing(raw));
}

std::uint32_t error_nodes_of(const types::language_result& r,
    const types::file_table& table, std::string_view path) {
    for (std::size_t row = 0; row < r.ids.size(); ++row) {
        if (table.paths[r.ids[row]] == path)
            return r.error_nodes[row];
    }
    FAIL("no row for " << path);
    return 0;
}

constexpr std::string_view valid_cpp =
    "#include <vector>\n"
    "namespace n {\n"
    "template <typename T> class box { T v_; public: T get() const; };\n"
    "int f(const std::vector<int>& v) { return v.empty() ? 0 : v[0]; }\n"
    "}\n";
constexpr std::string_view missing_semicolon_cpp = "int f() { return 1 }\n";
constexpr std::string_view nested_error_cpp = "int f( { int x = ; }\n";
constexpr std::string_view valid_sql =
    "CREATE TABLE t (a int, b text);\n"
    "SELECT a, b FROM t WHERE a = 1;\n";
constexpr std::string_view broken_sql = "SELECT a FROM t WHERE (a = (1;\n";

}

TEST_CASE("valid C++ has no error nodes", "[ops][parse]") {
    const auto c = parse(language::cpp, valid_cpp);
    CHECK(c.oracle == 0);
    CHECK(c.pruned == 0);
}

TEST_CASE("a missing token in C++ counts as one error node", "[ops][parse]") {
    const auto c = parse(language::cpp, missing_semicolon_cpp);
    CHECK(c.oracle == 1);
    CHECK(c.pruned == 1);
    CHECK(parse(language::cpp, missing_semicolon_cpp).pruned == c.pruned);
}

TEST_CASE("an error nested in an error counts once per node",
    "[ops][parse]") {
    const auto c = parse(language::cpp, nested_error_cpp);
    CHECK(c.oracle == 2);
    CHECK(c.pruned == 2);
    CHECK(parse(language::cpp, nested_error_cpp).pruned == c.pruned);
}

TEST_CASE("valid SQL has no error nodes", "[ops][parse]") {
    const auto c = parse(language::sql, valid_sql);
    CHECK(c.oracle == 0);
    CHECK(c.pruned == 0);
}

TEST_CASE("broken SQL has a stable error count", "[ops][parse]") {
    const auto c = parse(language::sql, broken_sql);
    CHECK(c.oracle == 1);
    CHECK(c.pruned == 1);
    CHECK(parse(language::sql, broken_sql).pruned == c.pruned);
}

TEST_CASE("errors in several statements add up", "[ops][parse]") {
    std::string text;
    for (int i = 0; i < 3; ++i) {
        text += valid_cpp;
        text += nested_error_cpp;
    }
    const auto c = parse(language::cpp, text);
    CHECK(c.pruned == c.oracle);
    CHECK(c.pruned >= 3);
}

TEST_CASE("files are routed to a language by extension", "[ops][parse]") {
    for (const auto e : {".hpp", ".cpp", ".h", ".hh", ".cc", ".cxx", ".ipp",
             ".tpp"})
        CHECK(ops::language_of(e) == language::cpp);
    CHECK(ops::language_of(".sql") == language::sql);
    for (const auto e : {"", ".org", ".txt", ".c", ".SQL", "sql", ".sqlx"})
        CHECK_FALSE(ops::language_of(e).has_value());
}

TEST_CASE("parse stage counts errors per file of a small checkout",
    "[ops][parse_stage]") {
    scoped_git_repo repo;
    repo.write("src/a.cpp", valid_cpp);
    repo.write("src/b.hpp", nested_error_cpp);
    repo.write("src/c.hpp", missing_semicolon_cpp);
    repo.write("sql/d.sql", valid_sql);
    repo.write("sql/e.sql", broken_sql);
    repo.write("README", nested_error_cpp);
    repo.track_all();

    const auto table = list(repo.root());
    const auto r = ops::run_parse_stage(repo.root(), table, 3, 2);
    const auto& cpp = r[language::cpp];
    const auto& sql = r[language::sql];

    CHECK(cpp.stage.files == 3);
    CHECK(sql.stage.files == 2);
    CHECK(cpp.stage.errors == 0);
    CHECK(cpp.stage.wall_ms.size() == 2);
    CHECK(sql.stage.wall_ms.size() == 2);
    CHECK(error_nodes_of(cpp, table, "src/a.cpp") == 0);
    CHECK(error_nodes_of(cpp, table, "src/b.hpp") == 2);
    CHECK(error_nodes_of(cpp, table, "src/c.hpp") == 1);
    CHECK(error_nodes_of(sql, table, "sql/d.sql") == 0);
    CHECK(error_nodes_of(sql, table, "sql/e.sql") == 1);
    CHECK(std::ranges::all_of(cpp.parse_ms, [](double ms) { return ms > 0; }));
    CHECK_FALSE(io::failed(r));

    std::ostringstream report;
    io::write_parse_report(report, r, {}, table);
    CHECK_THAT(report.str(), Catch::Matchers::ContainsSubstring(
        "cpp: files with parse errors: 2 of 3  error nodes: 3"));
    CHECK_THAT(report.str(), Catch::Matchers::ContainsSubstring(
        "sql: files with parse errors: 1 of 2  error nodes: 1"));
    CHECK_THAT(report.str(),
        !Catch::Matchers::ContainsSubstring("FAILED"));
}

TEST_CASE("parse stage notes a language with no files without failing",
    "[ops][parse_stage]") {
    scoped_git_repo repo;
    repo.write("src/a.cpp", valid_cpp);
    repo.track_all();

    const auto table = list(repo.root());
    const auto r = ops::run_parse_stage(repo.root(), table, 2, 1);

    CHECK(r[language::sql].stage.files == 0);
    CHECK_FALSE(io::failed(r));
    std::ostringstream report;
    io::write_parse_report(report, r, {}, table);
    CHECK_THAT(report.str(), Catch::Matchers::ContainsSubstring(
        "note: no sql files in the checkout."));
    CHECK_THAT(report.str(),
        !Catch::Matchers::ContainsSubstring("FAILED"));
}

TEST_CASE("parse stage fails loudly when no language has files",
    "[ops][parse_stage]") {
    scoped_git_repo repo;
    repo.write("readme.org", "* Title\n");
    repo.track_all();

    const auto table = list(repo.root());
    const auto r = ops::run_parse_stage(repo.root(), table, 2, 1);

    CHECK(io::failed(r));
    std::ostringstream report;
    io::write_parse_report(report, r, {}, table);
    CHECK_THAT(report.str(), Catch::Matchers::ContainsSubstring(
        "FAILED: stage 'parse' parsed zero files."));
}

TEST_CASE("parse stage fails loudly on a file it cannot read",
    "[ops][parse_stage]") {
    scoped_git_repo repo;
    repo.write("src/a.cpp", valid_cpp);
    repo.write("src/gone.hpp", valid_cpp);
    repo.write("sql/d.sql", valid_sql);
    repo.track_all();

    const auto table = list(repo.root());
    std::filesystem::remove(repo.root() / "src/gone.hpp");
    const auto r = ops::run_parse_stage(repo.root(), table, 2, 1);

    CHECK(r[language::cpp].stage.files == 1);
    CHECK(r[language::cpp].stage.errors == 1);
    CHECK(io::failed(r));
    std::ostringstream report;
    io::write_parse_report(report, r, {}, table);
    CHECK_THAT(report.str(), Catch::Matchers::ContainsSubstring(
        "FAILED: stage 'cpp' hit 1 error(s)."));
}

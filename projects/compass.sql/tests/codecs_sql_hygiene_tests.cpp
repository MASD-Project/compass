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
#include "compass.sql/codecs/sql_hygiene.hpp"

#include <array>
#include <map>
#include <string>
#include <tuple>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.sql/ops/hygiene.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass::sql;
namespace ct = compass::codec::types;
using files = std::map<std::string, std::string, std::less<>>;

/**
 * @brief A finding as path, rule and message.
 */
using finding = std::tuple<std::string, std::string, std::string>;

struct outcome {
    std::vector<finding> findings;
    std::uint64_t ran = 0;
    ct::dependency_record record;
};

outcome run(const files& all, const ct::configuration& extra = {},
    const ct::dependency_record& previous = {}) {
    ct::checkout_files checkout;
    for (const auto& [path, bytes] : all)
        checkout.paths.push_back(path);
    checkout.content_of = [&](std::string_view p) {
        return std::optional(compass::store::ops::blob_id_of(
            all.find(p)->second));
    };
    checkout.read = [&](std::string_view p) {
        return std::optional(all.find(p)->second);
    };
    ct::corpus corpus;
    corpus.files = &checkout;
    const compass::org::types::type_registry types;
    auto settings = extra;
    settings.set("sql_hygiene", "enabled", "true");
    const std::array<ct::descriptor, 1> codecs{codecs::sql_hygiene};
    const auto registry = compass::codec::ops::make_registry(codecs, types,
        settings);
    auto r = compass::codec::ops::run(registry, corpus, types, 1, previous);
    outcome out;
    const auto& t = r.diagnostics[0];
    for (std::uint32_t i = 0; i < t.size(); ++i) {
        const auto d = t[ct::diagnostic_id{i}];
        out.findings.emplace_back(std::string(d.file),
            std::string(codecs[0].rules[d.rule.value].name),
            std::string(d.message));
    }
    out.ran = r.ran[0];
    out.record = std::move(r.record);
    return out;
}

files tree() {
    return {
        {"projects/ores.sql/create/a.sql",
         "create table if not exists a (x int);\n"
         "/* open /* nested */\n"},
        {"projects/ores.sql/create/b.sql",
         "create index if not exists i on t (a, b);\n"
         "create index if not exists I on T (a,\n b);\n"},
        {"projects/ores.sql/create/clean.sql",
         "create or replace view v as select 'a' 'b';\n"},
        {"projects/ores.sql/notes.txt", "create table t (x int);\n"},
        {"projects/other/c.sql", "create table t (x int);\n"}};
}

std::vector<std::uint32_t> offsets(
    const std::vector<ops::not_re_runnable>& found) {
    std::vector<std::uint32_t> out;
    for (const auto& f : found)
        out.push_back(f.offset);
    return out;
}

}

TEST_CASE("each .sql file under the roots is checked and nothing else is",
    "[codecs][sql_hygiene]") {
    const auto r = run(tree());
    CHECK(r.findings == std::vector<finding>{
        {"projects/ores.sql/create/a.sql", "unterminated_block_comment",
         "a block comment is still open at the end of the file, so "
         "everything after it is comment"},
        {"projects/ores.sql/create/b.sql", "duplicate_index",
         "index i on t (a, b) is defined more than once"}});
}

TEST_CASE("the roots option names the directories checked",
    "[codecs][sql_hygiene]") {
    ct::configuration roots;
    roots.set("sql_hygiene", "roots", "projects/other,projects/none");
    const auto r = run(tree(), roots);
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<0>(r.findings[0]) == "projects/other/c.sql");
    CHECK(std::get<1>(r.findings[0]) == "not_re_runnable");
}

TEST_CASE("an unchanged tree reuses the findings and an edit runs again",
    "[codecs][sql_hygiene]") {
    const auto first = run(tree());
    CHECK(run(tree(), {}, first.record).ran == 0);
    auto fixed = tree();
    fixed["projects/ores.sql/create/a.sql"] += "*/\n";
    const auto again = run(fixed, {}, first.record);
    CHECK(again.ran == 1);
    CHECK(again.findings.size() == 1);
}

TEST_CASE("comments nest, and quotes, line comments and dollar bodies hide "
    "comment markers", "[ops][sql_hygiene]") {
    CHECK(ops::unterminated_comment("/* a /* b */ c */").depth == 0);
    const auto open = ops::unterminated_comment("x;\n/* a /* b */ c");
    CHECK(open.depth == 1);
    CHECK(open.offset == 3);
    CHECK(ops::unterminated_comment("'/*' -- /*\n$f$ /* $f$ \"/*\"")
        .depth == 0);
    CHECK(ops::unterminated_comment("'it''s /*'").depth == 0);
}

TEST_CASE("a duplicate index is the same name, table and columns, "
    "whatever the case and spacing", "[ops][sql_hygiene]") {
    const auto d = ops::duplicate_indexes(
        "create unique index if not exists x on \"t\" (a);\n"
        "CREATE UNIQUE INDEX IF NOT EXISTS X\n  ON t ( a );\n"
        "create index if not exists x on t (a);\n"
        "create index if not exists y on u (b);\n"
        "create index if not exists yon u (b);\n");
    REQUIRE(d.size() == 2);
    CHECK(d[0].name == "x");
    CHECK(d[0].table == "t");
    CHECK(d[0].columns == "a");
    CHECK(d[0].count == 3);
    CHECK(d[0].offset == 0);
    CHECK(d[1].name == "y");
    CHECK(d[1].count == 2);
}

TEST_CASE("a comment between two literals splits them, in dollar bodies "
    "too", "[ops][sql_hygiene]") {
    CHECK(ops::split_concatenations("select 'a' /* c */ 'b';") ==
        std::vector<std::uint32_t>{11});
    CHECK(ops::split_concatenations("select 'a' -- c\n 'b';") ==
        std::vector<std::uint32_t>{11});
    CHECK(ops::split_concatenations("select 'a'\n 'b' /* c */;").empty());
    CHECK(ops::split_concatenations("$$ x 'a' /* c */ 'b' $$") ==
        std::vector<std::uint32_t>{9});
}

TEST_CASE("masking blanks comments, strings and bodies and keeps quoted "
    "names and newlines", "[ops][sql_hygiene]") {
    CHECK(ops::mask_non_code("a /* b\nc */ 'd''e' \"f'g\" $q$h$q$ -- i\nj")
        == "a     \n            \"f'g\"             \nj");
}

TEST_CASE("DDL that cannot run twice is found in masked text only",
    "[ops][sql_hygiene]") {
    const std::string_view text =
        "create table t (x int);\n"
        "create table if not exists u (x int);\n"
        "create  table  if not exists w (x int);\n"
        "-- create table c (x int);\n"
        "create or replace function f() returns int as $$ create table b "
        "$$;\n"
        "create trigger tr before insert on t;\n"
        "create index i on t (x);\n"
        "create unique index concurrently if not exists j on t (x);\n"
        "drop policy if exists p on t;\ncreate policy p on t;\n"
        "create policy \"Q\" on t;\n"
        "drop type if exists k;\ncreate type k as enum ('a');\n"
        "create Domain d as int;\n";
    const auto found = ops::not_re_runnables(text);
    std::vector<std::string> why;
    for (const auto& f : found)
        why.push_back(f.why);
    CHECK(why == std::vector<std::string>{
        "create table without `if not exists`",
        "create table without `if not exists`",
        "create trigger without `or replace`",
        "create index without `if not exists`",
        "create policy \"Q\" with no `drop policy if exists`",
        "create Domain d with no `drop ... if exists`"});
    CHECK(offsets(found) == std::vector<std::uint32_t>{0, 62, 197, 235, 371,
        447});
}

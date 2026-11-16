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
#include "compass.cli/ops/ratchet.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass::cli;
using compass::store::ops::blob_id_of;

types::finding at(std::string path, std::uint32_t line,
    std::string message = "create table without `if not exists`") {
    types::finding f;
    f.path = std::move(path);
    f.line = line;
    f.rule = "sql_hygiene/not_re_runnable";
    f.message = std::move(message);
    return f;
}

types::finding drifted(std::string path, std::string_view file,
    std::string_view generated, std::string message) {
    types::finding f;
    f.path = std::move(path);
    f.line = 1;
    f.rule = "codegen/drift";
    f.message = std::move(message);
    f.drift = types::drift_sides{blob_id_of(file), blob_id_of(generated)};
    return f;
}

std::vector<std::string> fresh_of(const types::ratchet_split& s) {
    std::vector<std::string> out;
    for (const auto& f : s.fresh)
        out.push_back(f.path + ":" + std::to_string(f.line) + ": " +
            f.message);
    return out;
}

compass::store::types::commit_files files_of(
    std::vector<std::pair<std::string, std::string_view>> rows) {
    std::ranges::sort(rows);
    compass::store::types::commit_files out;
    for (const auto& [path, content] : rows) {
        out.paths.push_back(path);
        out.modes.push_back(0100644);
        out.ids.push_back(blob_id_of(content));
    }
    return out;
}

}

TEST_CASE("a finding that only moved is not new", "[ops][ratchet]") {
    const std::vector base{at("a.sql", 3), at("a.sql", 9)};
    const std::vector current{at("a.sql", 5), at("a.sql", 11)};
    const auto s = ops::split_new(base, current, {});
    CHECK(s.fresh.empty());
    CHECK(s.excused == 2);
}

TEST_CASE("a second copy of a finding is new, and the copy on a new line "
    "is the one reported", "[ops][ratchet]") {
    const std::vector base{at("a.sql", 3)};
    const std::vector current{at("a.sql", 3), at("a.sql", 7)};
    const auto s = ops::split_new(base, current, {});
    CHECK(fresh_of(s) == std::vector<std::string>{
        "a.sql:7: create table without `if not exists`"});
    CHECK(s.excused == 1);
}

TEST_CASE("a finding that moved and was copied is new once",
    "[ops][ratchet]") {
    const std::vector base{at("a.sql", 3)};
    const std::vector current{at("a.sql", 5), at("a.sql", 9)};
    const auto s = ops::split_new(base, current, {});
    CHECK(s.fresh.size() == 1);
    CHECK(s.excused == 1);
}

TEST_CASE("a finding with another rule, message or path is new",
    "[ops][ratchet]") {
    const std::vector base{at("a.sql", 3)};
    auto other_rule = at("a.sql", 3);
    other_rule.rule = "sql_hygiene/duplicate_index";
    const std::vector current{at("a.sql", 3), other_rule,
        at("a.sql", 3, "create view without `or replace`"), at("b.sql", 3)};
    const auto s = ops::split_new(base, current, {});
    CHECK(s.fresh.size() == 3);
    CHECK(s.excused == 1);
}

TEST_CASE("a removed finding excuses nothing else", "[ops][ratchet]") {
    const std::vector base{at("a.sql", 3), at("b.sql", 4)};
    const std::vector current{at("a.sql", 3), at("c.sql", 4)};
    const auto s = ops::split_new(base, current, {});
    CHECK(fresh_of(s) == std::vector<std::string>{
        "c.sql:4: create table without `if not exists`"});
}

TEST_CASE("a renamed file keeps its findings", "[ops][ratchet]") {
    const std::vector base{at("old.sql", 3)};
    const std::vector current{at("new.sql", 3)};
    CHECK(ops::split_new(base, current, {}).fresh.size() == 1);
    const std::vector renames{types::rename{"old.sql", "new.sql"}};
    CHECK(ops::split_new(base, current, renames).fresh.empty());
}

TEST_CASE("drift is excused only when neither side changed",
    "[ops][ratchet]") {
    const std::vector base{drifted("gen.hpp", "edited", "wanted",
        "the file differs from what codegen generates")};
    SECTION("same sides, another message") {
        const std::vector current{drifted("gen.hpp", "edited", "wanted",
            "the file was edited after compass wrote it")};
        CHECK(ops::split_new(base, current, {}).fresh.empty());
    }
    SECTION("what is generated changed") {
        const std::vector current{drifted("gen.hpp", "edited", "wanted 2",
            "the file differs from what codegen generates")};
        CHECK(ops::split_new(base, current, {}).fresh.size() == 1);
    }
    SECTION("the file changed") {
        const std::vector current{drifted("gen.hpp", "edited 2", "wanted",
            "the file differs from what codegen generates")};
        CHECK(ops::split_new(base, current, {}).fresh.size() == 1);
    }
}

TEST_CASE("exact renames pair a gone path with a new path holding its "
    "blob", "[ops][ratchet]") {
    const auto base = files_of({{"a.sql", "one"}, {"b.sql", "two"},
        {"c.sql", "three"}, {"d.sql", "same"}, {"e.sql", "same"}});
    const auto head = files_of({{"a.sql", "one"}, {"x/b.sql", "two"},
        {"c.sql", "three"}, {"x/c.sql", "three"}, {"y.sql", "same"},
        {"z.sql", "four"}});
    const std::vector<std::string_view> gone{"b.sql", "d.sql", "e.sql"};
    auto renames = ops::exact_renames(base, head, gone);
    std::ranges::sort(renames, {}, &types::rename::from);
    CHECK(renames == std::vector<types::rename>{{"b.sql", "x/b.sql"},
        {"d.sql", "y.sql"}});
}

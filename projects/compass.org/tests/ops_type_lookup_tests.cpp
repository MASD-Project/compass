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
#include "compass.org/ops/type_lookup.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::org::types;
namespace ops = compass::org::ops;

name_id intern(type_registry& r, std::string_view s) {
    for (std::uint32_t n = 0; n < r.names.size(); ++n) {
        if (ops::name_text(r, name_id{n}) == s)
            return {n};
    }
    r.names.firsts.values.push_back(
        static_cast<std::uint32_t>(r.name_bytes.size()));
    r.name_bytes.insert(r.name_bytes.end(), s.begin(), s.end());
    return {static_cast<std::uint32_t>(r.names.size() - 1)};
}

void add(type_registry& r, std::string_view name, type_kind kind,
    std::vector<std::string_view> active, std::vector<std::string_view> done,
    std::vector<std::string_view> sections) {
    const auto entry = [&] {
        return type_entry_id{static_cast<std::uint32_t>(r.entries.size())};
    };
    r.types.names.values.push_back(intern(r, name));
    r.types.kinds.values.push_back(kind);
    r.types.levels.values.push_back(type_level::none);
    r.types.folders.values.push_back(name_id::none());
    r.types.todo_firsts.values.push_back(entry());
    for (const auto s : active)
        r.entries.names.values.push_back(intern(r, s));
    r.types.done_firsts.values.push_back(entry());
    for (const auto s : done)
        r.entries.names.values.push_back(intern(r, s));
    r.types.section_firsts.values.push_back(entry());
    for (const auto s : sections)
        r.entries.names.values.push_back(intern(r, s));
}

std::vector<std::string> texts(const type_registry& r,
    column_view<type_entry_id, name_id> v) {
    std::vector<std::string> out;
    for (const auto n : v.values)
        out.emplace_back(ops::name_text(r, n));
    return out;
}

type_registry sample() {
    type_registry r;
    add(r, "knowledge", type_kind::document, {}, {}, {"Detail"});
    add(r, "ores.codegen.entity", type_kind::model, {}, {}, {});
    add(r, "story", type_kind::document, {"BACKLOG", "STARTED"},
        {"DONE", "ABANDONED"}, {"Goal", "Status"});
    add(r, "task", type_kind::document, {"STARTED"}, {"DONE"},
        {"Goal", "Status", "Result"});
    return r;
}

}

TEST_CASE("find_type finds every registered name and nothing else",
    "[ops][type_lookup]") {
    const auto r = sample();
    CHECK(ops::find_type(r, "knowledge") == type_id{0});
    CHECK(ops::find_type(r, "ores.codegen.entity") == type_id{1});
    CHECK(ops::find_type(r, "story") == type_id{2});
    CHECK(ops::find_type(r, "task") == type_id{3});

    CHECK(ops::find_type(r, "").is_none());
    CHECK(ops::find_type(r, "a").is_none());
    CHECK(ops::find_type(r, "stor").is_none());
    CHECK(ops::find_type(r, "storyx").is_none());
    CHECK(ops::find_type(r, "Task").is_none());
    CHECK(ops::find_type(r, "zzz").is_none());
    CHECK(ops::find_type(type_registry{}, "task").is_none());
}

TEST_CASE("entries of a type stop where the next type's start",
    "[ops][type_lookup]") {
    const auto r = sample();
    const auto story = ops::find_type(r, "story");
    CHECK(texts(r, ops::active_todos(r, story)) ==
        std::vector<std::string>{"BACKLOG", "STARTED"});
    CHECK(texts(r, ops::done_todos(r, story)) ==
        std::vector<std::string>{"DONE", "ABANDONED"});
    CHECK(texts(r, ops::required_sections(r, story)) ==
        std::vector<std::string>{"Goal", "Status"});

    const auto task = ops::find_type(r, "task");
    CHECK(texts(r, ops::required_sections(r, task)) ==
        std::vector<std::string>{"Goal", "Status", "Result"});
}

TEST_CASE("a type is stateful only when it has TODO keywords",
    "[ops][type_lookup]") {
    const auto r = sample();
    CHECK(ops::is_stateful(r, ops::find_type(r, "story")));
    CHECK_FALSE(ops::is_stateful(r, ops::find_type(r, "knowledge")));
    CHECK_FALSE(ops::is_stateful(r, ops::find_type(r, "ores.codegen.entity")));
}

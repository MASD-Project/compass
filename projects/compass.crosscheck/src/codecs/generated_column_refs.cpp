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
#include "compass.crosscheck/codecs/generated_column_refs.hpp"

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/words.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::uint32_t not_a_member = 0;
constexpr std::uint32_t not_a_column = 1;

constexpr std::array<ct::rule, 2> rules{{
    {"not_a_member", "What a generated table writer streams of its entity "
        "is a member of the entity."},
    {"not_a_column", "A generated index names only columns of the table it "
        "indexes, and a table the file creates."}}};

/**
 * @brief Whether the generator's marker is in the first 1500 characters
 * of @p text.
 */
bool generated(std::string_view text) {
    return text.substr(0, co::byte_of_character(text, 1500)).contains(
        "AUTO-GENERATED FILE");
}

std::string read(const ct::corpus_view& v, std::string_view path) {
    return pt::universal_newlines(pt::decode_replacing(
        v.read_file(path).value_or("")));
}

std::string without_comments(std::string_view text) {
    static const pt::regex comment(R"(/\*.*?\*/|//[^\n]*)", {.dotall = true});
    return comment.substitute(text, " ");
}

void table_writers(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex iterator(R"(const\s+auto&\s+(\w+)\s*:)");
    static const pt::regex access(R"(\b(\w+)\.(\w+)\b(?!\s*\())");
    static const pt::regex word(R"(\w+)");
    constexpr std::string_view fix = "Regenerate the writer and the entity "
        "from one model, or drop the name from the model's table display.";
    for (const auto path : co::glob(v,
             "projects/*/api/src/domain/*_table.cpp")) {
        const auto text = read(v, path);
        if (!generated(text))
            continue;
        const auto body = without_comments(text);
        const auto binding = iterator.search(body);
        if (!binding) {
            co::report(out, path, 0, not_a_member, "no `const auto& x : v` "
                "loop to check", fix);
            continue;
        }
        const std::string var(binding->group(1));
        const auto name = co::components_of(path).back();
        const auto entity = name.substr(0, name.size() -
            std::string_view("_table.cpp").size());
        const auto component = co::parent_of(co::parent_of(co::parent_of(
            path)));
        const auto headers = co::glob(v, std::string(component) +
            "/include/*/domain/" + std::string(entity) + ".hpp");
        if (headers.size() != 1) {
            co::report(out, path, 0, not_a_member, std::to_string(
                headers.size()) + " headers match " + std::string(entity) +
                ".hpp, expected 1", fix);
            continue;
        }
        std::set<std::string> members;
        for (const auto& m : word.find_all(without_comments(read(v,
                 headers.front()))))
            members.emplace(m.group());
        for (const auto& a : access.find_all(body)) {
            if (a.group(1) != var || members.contains(std::string(a.group(2))))
                continue;
            co::report(out, path, 0, not_a_member, "`" + var + "." +
                std::string(a.group(2)) + "` is not a member of " +
                std::string(entity), fix);
        }
    }
}

void sql_indexes(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex create_table(
        R"re(create\s+table\s+(?:if\s+not\s+exists\s+)?"?(ores_\w+_tbl)"?\s*\((.*?)\n\)\s*;)re",
        {.dotall = true, .ignorecase = true});
    static const pt::regex column_def(
        R"re(^\s*,?\s*"([a-z_][a-z0-9_]*)"\s+[a-z])re",
        {.multiline = true, .ignorecase = true});
    static const pt::regex create_index(
        R"re(create\s+(?:unique\s+)?index\s+(?:if\s+not\s+exists\s+)?"?(\w+)"?\s+on\s+"?(ores_\w+_tbl)"?\s*\((.*?)\))re",
        {.dotall = true, .ignorecase = true});
    static const pt::regex index_column(R"re("?([a-z_][a-z0-9_]*)"?)re");
    constexpr std::string_view fix = "Remove the column from the model's "
        "Indexes table, or add it to the entity.";
    for (const auto path : co::glob(v, "projects/ores.sql/create/**/*.sql")) {
        const auto text = read(v, path);
        if (!generated(text))
            continue;
        std::map<std::string, std::set<std::string>> columns;
        for (const auto& t : create_table.find_all(text)) {
            std::set<std::string> names;
            for (const auto& c : column_def.find_all(t.group(2)))
                names.emplace(ops::lowered(c.group(1)));
            columns[ops::lowered(t.group(1))] = std::move(names);
        }
        for (const auto& i : create_index.find_all(text)) {
            const std::string name(i.group(1));
            const std::string table(i.group(2));
            const auto known = columns.find(ops::lowered(table));
            if (known == columns.end()) {
                co::report(out, path, 0, not_a_column, "index " + name +
                    " on unknown table " + table, fix);
                continue;
            }
            for (const auto raw : ops::split(i.group(3), ',')) {
                const auto column = index_column.search(co::strip(raw));
                if (column && !known->second.contains(ops::lowered(
                        column->group(1))))
                    co::report(out, path, 0, not_a_column, "index " + name +
                        " names " + std::string(column->group(1)) + ", which "
                        "is not a column of " + table, fix);
            }
        }
    }
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    table_writers(v, out);
    sql_indexes(v, out);
}

}

const ct::descriptor generated_column_refs{
    .name = "generated_column_refs",
    .version = 1,
    .summary = "Reports each column a generated writer or index names that "
        "its entity or table does not have.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}

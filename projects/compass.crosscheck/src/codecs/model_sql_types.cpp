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
#include "compass.crosscheck/codecs/model_sql_types.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/glob.hpp"
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

constexpr std::uint32_t type_mismatch = 0;
constexpr std::uint32_t too_few_compared = 1;

constexpr std::array<ct::rule, 2> rules{{
    {"type_mismatch", "A column's model type is the type the table is "
        "created with."},
    {"too_few_compared", "At least min_compared columns are compared, so "
        "the check is reading the tree."}}};

constexpr std::array<ct::option, 1> options{{
    {"min_compared", "500", "The fewest columns a run may compare."}}};

const pt::regex& section() {
    static const pt::regex out(R"(^\* [^*\s].*$)");
    return out;
}

std::string read(const ct::corpus_view& v, std::string_view path) {
    return pt::universal_newlines(pt::decode_replacing(
        v.read_file(path).value_or("")));
}

/**
 * @brief The * Columns section of a model: each column heading's first
 * :type:, in order.
 */
std::vector<std::pair<std::string, std::string>> model_columns(
    std::string_view text) {
    static const pt::regex column(R"(^\*\* (\S+)\s*$)");
    static const pt::regex type(R"(^\s*:type:\s*(\S.*?)\s*$)");
    std::vector<std::pair<std::string, std::string>> out;
    bool in_columns = false;
    std::optional<std::string> name;
    for (const auto raw : co::split_lines(text)) {
        if (section().match(raw)) {
            if (co::strip(raw) == "* Columns") {
                in_columns = true;
                name.reset();
                continue;
            }
            if (in_columns)
                break;
            continue;
        }
        if (!in_columns)
            continue;
        if (const auto m = column.match(raw)) {
            name = std::string(m->group(1));
            continue;
        }
        const auto t = type.match(raw);
        if (!t || !name)
            continue;
        if (std::ranges::none_of(out, [&](const auto& c) {
                return c.first == *name; }))
            out.emplace_back(*name, std::string(t->group(1)));
    }
    return out;
}

const pt::regex& create_start() {
    static const pt::regex out(R"(^\s*create\s+table\s)",
        {.multiline = true, .ignorecase = true});
    return out;
}

/**
 * @brief The quoted columns of the first create table of @p text and the
 * type of each, its constraints dropped.
 */
std::map<std::string, std::string> created_columns(std::string_view text) {
    static const pt::regex create_column(
        R"re(^\s*"([A-Za-z0-9_]+)"\s+(.+?)\s*,?\s*$)re");
    static const pt::regex constraint(
        R"(\s+(not\s+null|null|default|primary\s+key|references|check|unique|generated|collate|constraint)\b)",
        {.ignorecase = true});
    std::map<std::string, std::string> out;
    bool inside = false;
    for (const auto raw : co::split_lines(text)) {
        if (!inside) {
            if (create_start().match(raw))
                inside = true;
            continue;
        }
        const auto stripped = co::strip(raw);
        if (stripped.starts_with(')'))
            break;
        if (stripped.empty() || stripped.starts_with("--"))
            continue;
        if (const auto m = create_column.match(raw)) {
            auto definition = m->group(2);
            if (const auto c = constraint.search(definition))
                definition = definition.substr(0, c->start());
            out[std::string(m->group(1))] = std::string(co::strip(definition));
        }
    }
    return out;
}

std::string normalise(std::string_view sql_type) {
    return ops::collapse_spaces(ops::lowered(co::strip(sql_type)));
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex tablename(R"(^\s*:tablename:\s*(\S+)\s*$)",
        {.multiline = true});
    static const pt::regex table_name(
        R"re(\s*(?:if\s+not\s+exists\s+)?"([A-Za-z0-9_]+)")re",
        {.ignorecase = true});
    std::map<std::string, std::string_view> create_files;
    for (const auto path : co::glob(v,
             "projects/ores.sql/create/**/*_create.sql")) {
        const auto text = read(v, path);
        const auto start = create_start().search(text);
        if (!start)
            continue;
        if (const auto name = table_name.match(text, start->end()))
            create_files[std::string(name->group(1))] = path;
    }

    std::size_t compared = 0;
    for (const auto path : co::glob(v, "projects/**/modeling/*.org")) {
        const auto text = read(v, path);
        const auto m = tablename.search(text);
        if (!m)
            continue;
        const auto sql_path = create_files.find(std::string(m->group(1)));
        if (sql_path == create_files.end())
            continue;
        const auto declared = model_columns(text);
        if (declared.empty())
            continue;
        const auto actual = created_columns(read(v, sql_path->second));
        for (const auto& [name, model_type] : declared) {
            const auto created = actual.find(name);
            if (created == actual.end())
                continue;
            ++compared;
            if (normalise(model_type) == normalise(created->second))
                continue;
            co::report(out, path, 0, type_mismatch, name + ": the model "
                "types it '" + model_type + "' but " +
                std::string(sql_path->second) + " creates it '" +
                created->second + "'", "Regenerate the component, then "
                "rebuild and recreate the database.");
        }
    }
    if (compared < std::stoul(std::string(v.option("min_compared"))))
        co::report(out, "projects", 0, too_few_compared, "compared only " +
            std::to_string(compared) + " columns, below the floor; the model "
            "or SQL format may have changed", "Teach the check the format, "
            "or lower min_compared for a subtree.");
}

}

const ct::descriptor model_sql_types{
    .name = "model_sql_types",
    .version = 1,
    .summary = "Reports each column whose model type is not the type its "
        "table is created with.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}

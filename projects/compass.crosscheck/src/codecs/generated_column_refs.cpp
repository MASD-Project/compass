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

#include <algorithm>
#include <array>
#include <string>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/generated_shape.hpp"
#include "compass.crosscheck/ops/words.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

enum rule : std::uint32_t { not_a_member, not_a_column };

constexpr std::array<ct::rule, 2> rules{{
    {"not_a_member", "What a generated table writer streams of its entity "
        "is a member of the entity."},
    {"not_a_column", "A generated index names only columns of the table it "
        "indexes."}}};

/**
 * @brief The offset in @p source of @p heading, a view into it, or 0 when
 * the model states no such heading.
 */
std::uint32_t offset_of(std::string_view source, std::string_view heading) {
    return heading.empty() ? 0
        : static_cast<std::uint32_t>(heading.data() - source.data());
}

void report(ct::diagnostic_table& out, const ct::document& d,
    std::string_view heading, rule r, std::string_view message,
    std::string_view fix) {
    const auto at = offset_of(d.source, heading);
    out.add({d.row, {at, at}, ct::rule_id{r}, ct::severity::error, message,
        fix});
}

/**
 * @brief The columns an index's column list names: each part between the
 * commas outside parentheses that is a column, quoted or not, with its
 * ordering after it. A part that is an expression names no column this
 * check can read.
 */
std::vector<std::string> indexed_columns(std::string_view list) {
    static const pt::regex column(
        R"re(^"?([A-Za-z_][A-Za-z0-9_]*)"?(?:\s+[A-Za-z_ ]+)?$)re");
    std::vector<std::string> out;
    int depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= list.size(); ++i) {
        if (i < list.size() && list[i] == '(')
            ++depth;
        else if (i < list.size() && list[i] == ')')
            --depth;
        else if (i == list.size() || (list[i] == ',' && depth == 0)) {
            const auto part = co::strip(list.substr(start, i - start));
            if (const auto m = column.match(part))
                out.push_back(ops::lowered(m->group(1)));
            start = i + 1;
        }
    }
    return out;
}

void run(const ct::document& d, const ct::corpus_view& v,
    ct::diagnostic_table& out) {
    const auto shape = ops::generated_shape_of(v, d);
    if (!shape)
        return;
    for (const auto& column : shape->display) {
        if (!shape->has_table_writer || shape->members.contains(column))
            continue;
        report(out, d, shape->display_heading, not_a_member, "`" +
            shape->iterator + "." + column + "` is not a member of " +
            shape->name, "Name a column of the model in its table display, "
            "or add the column to the model.");
    }
    if (!shape->creates_table)
        return;
    std::vector<std::string> table;
    for (const auto& c : shape->table_columns)
        table.push_back(ops::lowered(c));
    for (const auto& index : shape->indexes) {
        for (const auto& column : indexed_columns(index.columns)) {
            if (std::ranges::find(table, column) != table.end())
                continue;
            report(out, d, index.heading, not_a_column, "index " +
                index.name + " names " + column + ", which is not a column "
                "of " + shape->table, "Remove the column from the model's "
                "index, or add it to the entity.");
        }
    }
}

}

const ct::descriptor generated_column_refs{
    .name = "generated_column_refs",
    .version = 2,
    .summary = "Reports each column a model's table display or index names "
        "that its generated struct or table does not have.",
    .reads = ct::inputs::every_document(),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::document_function<ct::diagnostic_table>{run}};

}

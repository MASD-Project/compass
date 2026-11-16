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
#include "compass.codegen/ops/entity_steps.hpp"

#include <string>
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::value_id;

namespace {

/*
 * _format_detail_for_doxygen: the first line as is, every other line
 * behind the comment's continuation prefix.
 */
std::string doxygen_detail(std::string_view detail) {
    std::string out;
    std::size_t start = 0;
    for (;;) {
        const auto end = detail.find('\n', start);
        const auto line = detail.substr(start,
            end == std::string_view::npos ? std::string_view::npos
                                          : end - start);
        if (start == 0)
            out.append(line);
        else if (values::strip(line).empty())
            out.append("\n     *");
        else
            out.append("\n     * ").append(line);
        if (end == std::string_view::npos)
            return out;
        start = end + 1;
    }
}

}

void suppress_manual_coding_scheme(builder& b, value_id entity) {
    for (const auto c : values::items(b.tables(), entity, "columns")) {
        if (values::str(b.tables(), c, "name") == "coding_scheme_code") {
            b.set(entity, "has_coding_scheme", false);
            b.set(entity, "has_nullable_coding_scheme", false);
            return;
        }
    }
}

void mark_artefact_last_items(builder& b, value_id entity) {
    mark_last_item(b, values::member(b.tables(), entity, "artefact_indexes"));
    mark_last_item(b, values::member(b.tables(), entity, "artefact_columns"));
}

void derive_cardinality_limit_flag(builder& b, value_id entity) {
    const auto trigger = values::member(b.tables(), entity, "insert_trigger");
    for (const auto v : values::items(b.tables(), trigger, "validations")) {
        if (values::truthy(b.tables(), v, "cardinality_limit_table")) {
            b.set(b.ensure_object(entity, "sql"),
                "has_cardinality_limit_validations", true);
            return;
        }
    }
}

void format_columns(builder& b, value_id entity) {
    const auto columns = values::member(b.tables(), entity, "columns");
    mark_last_item(b, columns);
    for (const auto c : values::items(b.tables(), columns)) {
        const auto detail = values::member(b.tables(), c, "detail");
        const std::string text(values::str(b.tables(), detail));
        if (text.find('\n') != std::string::npos)
            b.set(c, "detail", doxygen_detail(text));
    }
}

}

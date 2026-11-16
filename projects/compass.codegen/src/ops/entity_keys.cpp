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

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include "compass.codegen/ops/values.hpp"
#include "entity_text.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

bool is_word_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

/*
 * Whether @p text names idx as a word, as re.search(r'\bidx\b') finds it.
 */
bool names_idx(std::string_view text) {
    for (auto at = text.find("idx"); at != std::string_view::npos;
         at = text.find("idx", at + 1)) {
        const bool starts = at == 0 || !is_word_char(text[at - 1]);
        const bool ends = at + 3 == text.size() || !is_word_char(text[at + 3]);
        if (starts && ends)
            return true;
    }
    return false;
}

std::string join_columns(const model_tables& t,
    const std::vector<value_id>& keys, std::string_view separator) {
    std::string out;
    for (const auto k : keys) {
        if (!out.empty())
            out += separator;
        out += values::str(t, k, "column");
    }
    return out;
}

bool any_of(const model_tables& t, const std::vector<value_id>& v,
    std::string_view flag) {
    return std::ranges::any_of(v,
        [&](value_id c) { return values::truthy(t, c, flag); });
}

}

void derive_natural_key_flags(builder& b, value_id entity) {
    const auto& t = b.tables();
    if (values::member(t, entity, "natural_keys").is_none()) {
        if (values::member(t, entity, "has_text_natural_keys").is_none())
            b.set(entity, "has_text_natural_keys", false);
        return;
    }
    const auto keys = values::items(t, entity, "natural_keys");
    if (!keys.empty() && values::is(t, keys.back(),
            compass::model::types::value_kind::object)) {
        b.set(keys.back(), "last",
            !values::truthy(t, entity, "columns"));
    }

    const auto iter_var = values::member(t,
        values::member(t, entity, "cpp"), "iterator_var");
    for (const auto k : keys) {
        const std::string type(values::str(t, k, "type"));
        const std::string cpp_type(values::str(t, k, "cpp_type"));
        const bool nullable = values::truthy(t, k, "nullable");
        const bool is_uuid = type == "uuid" ||
            cpp_type.contains("boost::uuids::uuid");
        const bool is_timestamp = cpp_type.contains("time_point") ||
            type == "timestamp" || type == "timestamptz" ||
            type == "timestamp with time zone";
        if (iter_var.is_none())
            b.set(k, "iter_var", "e");
        else
            b.set_copy(k, "iter_var", iter_var);
        b.set(k, "is_uuid", is_uuid);
        b.set(k, "is_int", cpp_type == "int" || cpp_type == "long" ||
            cpp_type == "std::size_t" || type == "integer");
        b.set(k, "is_timestamp", is_timestamp);
        b.set(k, "is_date", cpp_type == "std::chrono::year_month_day" ||
            type == "date");
        b.set(k, "is_optional_uuid", is_uuid && nullable);
        b.set(k, "is_optional_timestamp", is_timestamp && nullable);
        b.set(k, "render_label", text::render_label(values::str(t, k, "column")));
    }

    b.set(entity, "has_multiple_natural_keys", keys.size() > 1);
    const bool text_key = std::ranges::any_of(keys, [&](value_id k) {
        return !values::truthy(t, k, "is_uuid") &&
            !values::truthy(t, k, "is_int") &&
            !values::truthy(t, k, "is_timestamp") &&
            !values::truthy(t, k, "is_date") &&
            !(values::truthy(t, k, "generator_expr") &&
                values::truthy(t, k, "no_generator_suffix"));
    });
    auto generated = keys;
    const auto columns = values::items(t, entity, "columns");
    generated.insert(generated.end(), columns.begin(), columns.end());
    const bool idx_generator = std::ranges::any_of(generated,
        [&](value_id c) {
            return names_idx(values::str(t, c, "generator_expr"));
        });
    b.set(entity, "has_text_natural_keys", text_key || idx_generator);
    b.set(entity, "has_date_natural_keys", any_of(t, keys, "is_date"));
    b.set(entity, "has_date_or_timestamp_natural_keys",
        any_of(t, keys, "is_date") || any_of(t, keys, "is_timestamp"));
    if (keys.size() > 1) {
        b.set(entity, "natural_keys_composite_columns",
            join_columns(t, keys, ", "));
        if (values::member(t, entity, "natural_keys_composite_name").is_none())
            b.set(entity, "natural_keys_composite_name",
                join_columns(t, keys, "_"));
    }
}

void derive_key_aggregates(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto columns = values::items(t, entity, "columns");

    const auto unique = b.list(entity, "unique_keys");
    for (const auto c : columns) {
        if (values::is_true(t, c, "unique"))
            b.set_copy(b.append_object(unique), "column",
                values::member(b.tables(), c, "name"));
    }

    b.set(entity, "has_date_or_timestamp_natural_keys",
        values::truthy(t, entity, "has_date_or_timestamp_natural_keys") ||
        any_of(t, columns, "is_required_timestamp") ||
        std::ranges::any_of(columns, [&](value_id c) {
            return values::truthy(t, c, "is_optional_timestamp") &&
                values::truthy(t, c, "render_is_timestamp");
        }));
    const bool unique_suffix = any_of(t, columns, "requires_unique_suffix");
    b.set(entity, "has_unique_suffix_columns", unique_suffix);

    const auto pk = values::member(t, entity, "primary_key");
    b.set(entity, "needs_counter", values::str(t, pk, "type") == "text" ||
        values::truthy(t, entity, "has_text_natural_keys") || unique_suffix);

    std::vector<value_id> generated;
    if (!pk.is_none())
        generated.push_back(pk);
    const auto keys = values::items(t, entity, "natural_keys");
    generated.insert(generated.end(), keys.begin(), keys.end());
    generated.insert(generated.end(), columns.begin(), columns.end());
    const auto extra = values::items(t, pk, "extra_columns");
    generated.insert(generated.end(), extra.begin(), extra.end());
    b.set(entity, "has_uuid_to_string_generator",
        std::ranges::any_of(generated, [&](value_id c) {
            return values::str(t, c, "generator_expr")
                .contains("boost::uuids::to_string");
        }));
}

}

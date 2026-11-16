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

#include <cstdint>
#include <string>
#include <vector>
#include "compass.codegen/ops/values.hpp"
#include "entity_text.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;
using compass::model::types::value_kind;

namespace {

/*
 * str(spec).split(): the words of an order spec.
 */
std::vector<std::string> words_of(std::string_view s) {
    std::vector<std::string> out;
    constexpr std::string_view space = " \t\n\r\f\v";
    for (auto start = s.find_first_not_of(space);
         start != std::string_view::npos;
         start = s.find_first_not_of(space, start)) {
        const auto end = s.find_first_of(space, start);
        out.emplace_back(s.substr(start, end == std::string_view::npos
            ? std::string_view::npos : end - start));
        if (end == std::string_view::npos)
            break;
        start = end;
    }
    return out;
}

/*
 * An order spec, "<column> [asc|desc]"; the loader's models never state
 * any other shape.
 */
struct order_spec {
    std::string column;
    bool desc = false;
};

order_spec parse_order(std::string_view spec) {
    const auto words = words_of(spec);
    if (words.empty())
        return {};
    return {words[0], words.size() == 2 && text::lower(words[1]) == "desc"};
}

/*
 * _cpp_string_list: a C++ braced list of string literals.
 */
std::string cpp_string_list(const std::vector<std::string>& names) {
    std::string out = "{";
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != 0)
            out += ", ";
        out.append("\"").append(names[i]).append("\"");
    }
    return out + "}";
}

std::int64_t default_limit(const model_tables& t, value_id fk) {
    const auto v = values::member(t, fk, "list_by_default_limit");
    if (values::is(t, v, value_kind::integer))
        return t.values.scalars[v];
    if (values::is(t, v, value_kind::string))
        return std::stoll(std::string(values::strip(values::str(t, v))));
    return 100;
}

std::string name_or_column(const model_tables& t, value_id c) {
    std::string name(values::str(t, c, "column"));
    return name.empty() ? std::string(values::str(t, c, "name")) : name;
}

}

void derive_list_requests(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto pk = values::member(t, entity, "primary_key");
    const auto pk_column = values::member(t, pk, "column");
    const std::string pk_col(pk_column.is_none() ? std::string_view("id")
                                                 : values::str(t, pk_column));
    const auto short_plural = values::member(t, entity, "entity_plural_short");
    const auto plural = values::py_str(t, short_plural.is_none()
        ? values::member(t, entity, "entity_plural") : short_plural, "None");

    const auto requests = b.list(entity, "extra_list_requests");
    for (const auto fk : values::items(t, entity, "foreign_keys")) {
        if (!values::truthy(t, fk, "list_by"))
            continue;
        const std::string column(values::str(t, fk, "column"));
        const auto limit = default_limit(t, fk);
        const auto r = b.append_object(requests);
        b.set(r, "name_suffix", "by_" + column);
        b.set(r, "nats_suffix", "list_by_" + column);
        b.set(r, "filter_column", column);
        b.set(r, "service_method", "list_" + plural + "_by_" + column);
        b.set(r, "count_service_method", "count_" + plural + "_by_" + column);
        b.set_integer(r, "default_limit", limit);
    }
    for (const auto fk : values::items(t, entity, "foreign_keys")) {
        if (!values::truthy(t, fk, "list_by"))
            continue;
        const auto stated = values::member(t, fk, "list_by_order_by");
        const auto words = words_of(stated.is_none()
            ? std::string_view(pk_col) : values::str(t, stated));
        b.set(fk, "list_by_order_column", words.empty() ? "" : words[0]);
        b.set(fk, "list_by_order_desc",
            words.size() > 1 && text::lower(words[1]) == "desc");
    }
}

void apply_stated_order(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto pk = values::member(t, entity, "primary_key");
    std::vector<std::string> key;
    for (const auto c : values::items(t, pk, "columns"))
        key.emplace_back(values::str(t, c, "column"));
    if (key.empty()) {
        const auto column = values::member(t, pk, "column");
        key.emplace_back(column.is_none() ? std::string_view("id")
                                          : values::str(t, column));
    }

    std::vector<value_id> columns = values::items(t, pk, "columns");
    for (const auto key_list : {"natural_keys", "columns"}) {
        const auto more = values::items(t, entity, key_list);
        columns.insert(columns.end(), more.begin(), more.end());
    }
    struct sortable_field {
        std::string name;
        std::string column;
    };
    std::vector<sortable_field> sortable;
    for (const auto c : columns) {
        if (values::is_true(t, c, "sortable")) {
            const auto name = name_or_column(t, c);
            sortable.push_back({name, name});
        }
    }
    if (values::is_true(t, entity, "sortable_recorded_at"))
        sortable.push_back({"recorded_at", "valid_from"});

    const auto fields = b.list(entity, "sortable_fields");
    for (const auto& f : sortable) {
        const auto o = b.append_object(fields);
        b.set(o, "name", f.name);
        b.set(o, "column", f.column);
    }
    b.set(entity, "order_key_columns", cpp_string_list(key));

    auto default_columns = key;
    bool desc = false;
    if (values::truthy(t, entity, "default_order")) {
        const auto spec = parse_order(values::py_str(t,
            values::member(t, entity, "default_order")));
        for (const auto& f : sortable) {
            if (f.name == spec.column)
                default_columns = {f.column};
        }
        desc = spec.desc;
    }
    const auto order_columns = cpp_string_list(default_columns);
    const std::string order_desc = desc ? "true" : "false";
    b.set(entity, "default_order_columns", order_columns);
    b.set(entity, "default_order_desc", order_desc);

    for (const auto fk : values::items(t, entity, "foreign_keys")) {
        if (!values::truthy(t, fk, "list_by"))
            continue;
        if (values::truthy(t, fk, "list_by_order_by")) {
            const auto spec = parse_order(values::py_str(t,
                values::member(t, fk, "list_by_order_by")));
            b.set(fk, "list_by_default_columns", cpp_string_list({spec.column}));
            b.set(fk, "list_by_default_desc", spec.desc ? "true" : "false");
        } else {
            b.set(fk, "list_by_default_columns", order_columns);
            b.set(fk, "list_by_default_desc", order_desc);
        }
    }
}

}

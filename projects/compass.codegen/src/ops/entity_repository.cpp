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

#include <map>
#include <set>
#include <string>
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

/*
 * The column paths the table display streams specially: uuids through
 * to_string, optionals unwrapped, plain bools as words.
 */
struct display_types {
    std::set<std::string, std::less<>> uuid;
    std::set<std::string, std::less<>> optional;
    std::set<std::string, std::less<>> boolean;

    void classify(std::string_view cpp_type, std::string_view path) {
        const auto cpp = values::strip(cpp_type);
        const bool is_optional = cpp.starts_with("std::optional<");
        if (cpp.contains("boost::uuids::uuid") && !is_optional)
            uuid.emplace(path);
        if (is_optional)
            optional.emplace(path);
        if (cpp == "bool")
            boolean.emplace(path);
    }
};

display_types display_types_of(const model_tables& t, value_id entity) {
    display_types out;
    const auto pk = values::member(t, entity, "primary_key");
    if (values::truthy(t, pk, "is_uuid"))
        out.uuid.emplace(values::str(t, pk, "column"));
    for (const auto k : values::items(t, entity, "natural_keys")) {
        if (values::truthy(t, k, "is_uuid"))
            out.uuid.emplace(values::str(t, k, "column"));
    }
    for (const auto c : values::items(t, entity, "columns"))
        out.classify(values::str(t, c, "cpp_type"), values::str(t, c, "name"));
    for (const auto f : values::items(t, entity, "domain_group_fields"))
        out.classify(values::str(t, f, "cpp_type"), values::str(t, f, "name"));
    return out;
}

/*
 * _table_display_members: the member prefix a grouped entity reaches a
 * column through, by column name.
 */
std::map<std::string, std::string, std::less<>> display_members(
    const model_tables& t, value_id entity) {
    std::map<std::string, std::string, std::less<>> out;
    for (const auto f : values::items(t, entity, "domain_group_fields")) {
        const auto name = values::str(t, f, "name");
        const auto dot = name.find('.');
        if (dot != std::string_view::npos && dot + 1 < name.size())
            out[std::string(name.substr(dot + 1))] =
                std::string(name.substr(0, dot)) + ".";
    }
    return out;
}

}

void derive_table_display(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto cpp = values::member(t, entity, "cpp");
    if (cpp.is_none())
        return;
    const auto display = values::member(t, cpp, "table_display");
    if (display.is_none())
        return;
    const auto types = display_types_of(t, entity);
    const auto members = display_members(t, entity);
    const auto iter_var = values::member(t, cpp, "iterator_var");

    bool has_uuid = false;
    bool has_optional = false;
    for (const auto item : values::items(t, display)) {
        if (iter_var.is_none())
            b.set(item, "iter_var", "e");
        else
            b.set_copy(item, "iter_var", iter_var);
        std::string column(values::str(t, item, "column"));
        if (!members.empty() && !column.contains('.')) {
            const auto m = members.find(column);
            column = (m == members.end() ? std::string() : m->second) + column;
            b.set(item, "column", column);
        }
        const bool is_uuid = types.uuid.contains(column);
        const bool is_optional = types.optional.contains(column);
        b.set(item, "is_uuid", is_uuid);
        b.set(item, "is_optional", is_optional);
        b.set(item, "is_bool", types.boolean.contains(column) && !is_optional);
        has_uuid = has_uuid || is_uuid;
        has_optional = has_optional || is_optional;
    }
    b.set(cpp, "has_uuid_table_display", has_uuid);
    b.set(cpp, "has_optional_table_display", has_optional);
}

void copy_repository_fields(builder& b, value_id entity) {
    const auto repository = values::member(b.tables(), entity, "repository");
    for (const auto field : values::items(b.tables(), repository))
        b.set_copy(entity, std::string(values::key_of(b.tables(), field)),
            field);
}

void derive_tenant_scope(builder& b, value_id entity) {
    const auto& t = b.tables();
    b.set(entity, "read_tenant_filtered",
        values::truthy(t, entity, "has_tenant_id") &&
        values::str(t, entity, "tenant_read_scope") != "shared");
    const bool system_visible =
        values::truthy(t, entity, "system_tenant_visible");
    b.set(entity, "system_tenant_visible", system_visible);
    if (system_visible) {
        b.set(entity, "tenant_where",
            R"(("tenant_id"_c == tid || "tenant_id"_c == sys))");
        b.set(entity, "sys_decl",
            "\n    static const std::string sys("
            "ores::database::service::tenant_context::system_tenant_id);");
    } else {
        b.set(entity, "tenant_where", R"("tenant_id"_c == tid)");
        b.set(entity, "sys_decl", "");
    }
}

void derive_messaging_defaults(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto pk = values::member(t, entity, "primary_key");
    const auto column = values::member(t, pk, "column");
    const std::string pk_col(column.is_none() ? std::string_view("id")
                                              : values::str(t, column));
    const auto type = values::member(t, pk, "type");
    const bool text_key = type.is_none() ? false
                                         : values::str(t, type) == "text";
    const auto absent = [&](std::string_view key) {
        return values::member(b.tables(), entity, key).is_none();
    };

    if (absent("delete_request_id_field"))
        b.set(entity, "delete_request_id_field", text_key ? pk_col + "s" : "ids");
    if (absent("history_request_id_field")) {
        if (column.is_none())
            b.set(entity, "history_request_id_field", "id");
        else
            b.set_copy(entity, "history_request_id_field", column);
    }
    auto extra = values::items(t, pk, "columns");
    if (!extra.empty())
        extra.erase(extra.begin());
    if (absent("history_request_extra_args")) {
        const auto list = b.list(entity, "history_request_extra_args");
        for (const auto c : extra)
            b.set_copy(b.append_object(list), "name",
                values::member(b.tables(), c, "column"));
    }
    if (absent("delete_request_extra_args")) {
        const auto list = b.list(entity, "delete_request_extra_args");
        for (const auto c : extra)
            b.set(b.append_object(list), "name",
                std::string(values::str(b.tables(), c, "column")) + "s");
    }
    if (absent("single_delete"))
        b.set(entity, "single_delete", false);
}

}

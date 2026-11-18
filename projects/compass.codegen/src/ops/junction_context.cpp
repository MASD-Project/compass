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
#include "compass.codegen/ops/model_contexts.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>
#include "compass.codegen/ops/entity_steps.hpp"
#include "compass.codegen/ops/values.hpp"
#include "compass.model/ops/typescript.hpp"
#include "entity_text.hpp"
#include "entity_wire.hpp"
#include "model_steps.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

constexpr std::string_view year_month_day = "std::chrono::year_month_day";

/*
 * Python's `flag and model.get(key, False)`: false when @p flag is,
 * otherwise the stated value of @p key, false when it states none.
 */
void set_and(builder& b, value_id o, std::string_view key, bool flag,
    value_id stated) {
    if (!flag || stated.is_none())
        b.set(o, key, false);
    else
        b.set_copy(o, key, stated);
}

/*
 * The type flags and iterator variable of each of the junction's own
 * columns, and has_base64_columns.
 */
void flag_columns(builder& b, value_id junction) {
    const auto& t = b.tables();
    const auto iter_var = values::member(t,
        values::member(t, junction, "cpp"), "iterator_var");
    const auto columns = values::items(t, junction, "columns");
    bool base64 = false;
    for (const auto c : columns) {
        const auto type = values::str(b.tables(), c, "type");
        const auto cpp_type = values::str(b.tables(), c, "cpp_type");
        const auto nullable = values::member(b.tables(), c, "nullable");
        const bool is_nullable = values::truthy(b.tables(), nullable);
        b.set(c, "is_int", type == "integer" || cpp_type == "int");
        const bool uuid = type == "uuid" ||
            cpp_type.find("boost::uuids::uuid") != std::string_view::npos;
        b.set(c, "is_uuid", uuid && !is_nullable);
        set_and(b, c, "is_optional_uuid", uuid, nullable);
        b.set(c, "is_date", type == "date" || cpp_type == year_month_day);
        const bool stamp = type == "timestamp" || type == "timestamptz" ||
            type == "timestamp with time zone";
        b.set(c, "is_timestamp", stamp);
        set_and(b, c, "is_optional_timestamp", stamp, nullable);
        b.set(c, "is_required_timestamp", stamp && !is_nullable);
        const auto declared = values::member(b.tables(), c, "base64");
        const bool is_base64 = values::is(b.tables(), declared,
            compass::model::types::value_kind::boolean) &&
            values::truthy(b.tables(), declared) && !is_nullable;
        b.set(c, "is_base64", is_base64);
        base64 = base64 || is_base64;
        if (iter_var.is_none())
            b.set(c, "iter_var", "m");
        else
            b.set_copy(c, "iter_var", iter_var);
    }
    if (!values::member(b.tables(), junction, "columns").is_none())
        b.set(junction, "has_base64_columns", base64);
}

void flag_sides(builder& b, value_id junction) {
    for (const auto key : {"left", "right"}) {
        const auto side = values::member(b.tables(), junction, key);
        if (side.is_none())
            continue;
        if (!values::member(b.tables(), side, "column_title").is_none())
            b.set(side, "column_title_lower",
                text::lower(values::str(b.tables(), side, "column_title")));
        const auto type = values::str(b.tables(), side, "type");
        b.set(side, "is_uuid", type == "uuid");
        b.set(side, "is_date", type == "date" ||
            values::str(b.tables(), side, "cpp_type") == year_month_day);
    }
    const auto& t = b.tables();
    b.set(junction, "has_uuid_left_or_right",
        values::truthy(t, values::member(t, junction, "left"), "is_uuid") ||
        values::truthy(t, values::member(t, junction, "right"), "is_uuid"));
}

/*
 * history_value_expression: the C++ expression that renders @p member of
 * type @p cpp_type as text.
 */
std::string history_value(std::string_view cpp_type, const std::string& member,
    std::string_view owner) {
    const auto type = values::strip(cpp_type);
    constexpr std::string_view optional = "std::optional<";
    if (type.starts_with(optional) && type.ends_with('>')) {
        const auto inner = history_value(
            type.substr(optional.size(), type.size() - optional.size() - 1),
            "(*" + member + ")", owner);
        return "(" + member + " ? " + inner + " : std::string())";
    }
    if (type == "std::string")
        return member;
    if (type == "int")
        return "std::to_string(" + member + ")";
    if (type == "bool")
        return "(" + member +
            " ? std::string(\"true\") : std::string(\"false\"))";
    if (type == "boost::uuids::uuid")
        return "boost::uuids::to_string(" + member + ")";
    if (type == year_month_day)
        return "ores::platform::time::datetime::to_iso8601_date(" + member +
            ")";
    if (type == "std::chrono::system_clock::time_point")
        return "ores::platform::time::datetime::to_iso8601_utc(" + member +
            ")";
    throw std::runtime_error(std::string(owner) +
        ": a junction history cannot render " + member + " of type '" +
        std::string(type) + "'");
}

/*
 * derive_junction_history, less history_parent and history_child, which
 * share the sides and are copied once the sides are final.
 */
void derive_history(builder& b, value_id junction) {
    const auto& t = b.tables();
    const std::string owner(values::member(t, junction, "name_singular")
        .is_none() ? "?" : values::str(t, junction, "name_singular"));
    const auto product = values::member(t, junction, "product").is_none()
        ? std::string("ores") : std::string(values::str(t, junction, "product"));
    const std::string component(values::str(t, junction, "component"));
    b.set(junction, "history_entity_type", product + "." + component + "." +
        std::string(values::str(t, junction, "name_singular")));
    if (!values::truthy(b.tables(), junction, "history_read_permission"))
        b.set(junction, "history_read_permission", component + "::" +
            std::string(values::str(b.tables(), junction, "name")) + ":read");
    for (const auto key : {"left", "right"}) {
        const auto side = values::member(b.tables(), junction, key);
        if (side.is_none())
            continue;
        b.set(side, "history_value", history_value(
            values::str(b.tables(), side, "cpp_type"),
            "v.link." + std::string(values::str(b.tables(), side, "column")),
            owner));
    }
    for (const auto c : values::items(b.tables(), junction, "columns")) {
        const std::string name(values::str(b.tables(), c, "name"));
        auto spaced = name;
        std::ranges::replace(spaced, '_', ' ');
        b.set(c, "history_title", text::title(spaced));
        b.set(c, "history_value", history_value(
            values::str(b.tables(), c, "cpp_type"), "v.link." + name, owner));
    }
}

/*
 * history_parent and history_child: the side the history is read by and
 * the side that names the link within it.
 */
void copy_history_sides(builder& b, value_id junction) {
    const auto& t = b.tables();
    const auto left = values::member(t, junction, "left");
    const auto right = values::member(t, junction, "right");
    const bool parent_is_right = values::truthy(t, right, "list_by") &&
        !values::truthy(t, left, "list_by");
    const auto parent = parent_is_right ? right : left;
    const auto child = parent_is_right ? left : right;
    if (parent.is_none())
        b.object(junction, "history_parent");
    else
        b.set_copy(junction, "history_parent", parent);
    if (child.is_none())
        b.object(junction, "history_child");
    else
        b.set_copy(junction, "history_child", child);
}

/*
 * derive_junction_events: the notify channel, the key columns the trigger
 * spells and whether the envelope carries the party.
 */
void derive_announcements(builder& b, value_id junction) {
    const auto& t = b.tables();
    const auto product = values::member(t, junction, "product").is_none()
        ? std::string("ores") : std::string(values::str(t, junction, "product"));
    const auto stated = values::str(t, values::member(t, junction, "sql"),
        "tablename");
    const auto table = !stated.empty() ? std::string(stated) : product + "_" +
        std::string(values::str(t, junction, "component")) + "_" +
        std::string(values::str(t, junction, "name")) + "_tbl";
    b.set(junction, "events_channel", table.substr(0, table.size() - 4));
    const std::array<std::string, 2> columns{
        std::string(values::str(b.tables(),
            values::member(b.tables(), junction, "left"), "column")),
        std::string(values::str(b.tables(),
            values::member(b.tables(), junction, "right"), "column"))};
    const auto list = b.list(junction, "event_key_columns");
    for (std::size_t i = 0; i < columns.size(); ++i) {
        const auto o = b.append_object(list);
        b.set(o, "column", columns[i]);
        b.set(o, "last", i + 1 == columns.size());
    }
    b.set(junction, "has_party_column",
        std::ranges::find(columns, "party_id") != columns.end());
}

/*
 * The include and conversion gates the mapper, repository and generator
 * read.
 */
void derive_include_gates(builder& b, value_id junction) {
    const auto& t = b.tables();
    const auto columns = values::items(t, junction, "columns");
    const auto any = [&](std::string_view key) {
        return std::ranges::any_of(columns,
            [&](value_id c) { return values::truthy(t, c, key); });
    };
    const auto left = values::member(t, junction, "left");
    const auto right = values::member(t, junction, "right");
    const bool side_date = values::truthy(t, left, "is_date") ||
        values::truthy(t, right, "is_date");
    const bool dated = side_date || any("is_date");
    const auto side_str = [&](value_id side) {
        return values::truthy(t, side, "query_needs_str") &&
            !values::truthy(t, side, "is_uuid");
    };
    const auto counts = [&](value_id o) {
        return values::str(t, o, "generator_expr").find("idx") !=
            std::string_view::npos;
    };
    const bool needs_counter = counts(left) || counts(right) ||
        std::ranges::any_of(columns, counts);
    const bool optional = any("is_optional_timestamp");
    const bool string_view = any("is_required_timestamp");
    const bool stamped = any("is_timestamp");
    const bool repository_datetime = side_str(left) || side_str(right);
    b.set(junction, "has_date_left_or_right_or_column", dated);
    b.set(junction, "mapper_uses_optional", optional);
    b.set(junction, "mapper_uses_string_view", string_view);
    b.set(junction, "repository_uses_chrono", side_date);
    b.set(junction, "mapper_uses_datetime", dated || stamped);
    b.set(junction, "repository_uses_datetime", repository_datetime);
    b.set(junction, "needs_counter", needs_counter);
}

void derive_member_ts_types(builder& b, value_id junction,
    const types::entity_registry& registry) {
    std::vector<value_id> members;
    for (const auto key : {"left", "right"}) {
        const auto side = values::member(b.tables(), junction, key);
        if (!side.is_none())
            members.push_back(side);
    }
    std::ranges::copy(values::items(b.tables(), junction, "columns"),
        std::back_inserter(members));
    for (const auto m : members) {
        const auto mapped = compass::model::ops::ts_domain_type(
            values::str(b.tables(), m, "cpp_type"), registry.custom_types);
        if (!mapped.empty())
            b.set(m, "ts_type", mapped);
    }
    if (!values::member(b.tables(), junction, "name_singular").is_none())
        b.set(junction, "name_pascal", compass::model::ops::pascal_case(
            values::str(b.tables(), junction, "name_singular")));
}

/*
 * junction_entity_shape: the junction projected onto the entity shape, its
 * key both sides, its plural its name.
 */
value_id junction_entity_shape(builder& b, value_id junction) {
    const auto shape = b.detached_object();
    const auto& t = b.tables();
    const auto name = values::py_str(t, values::member(t, junction, "name"), "");
    b.set(shape, "component",
        values::py_str(b.tables(), values::member(b.tables(), junction,
            "component"), ""));
    for (const auto key : {"http_area", "web_resource", "web_list_by"}) {
        const auto v = values::member(b.tables(), junction, key);
        if (values::truthy(b.tables(), v))
            b.set_copy(shape, key, v);
    }
    const auto read_only = values::member(b.tables(), junction, "web_read_only");
    if (!read_only.is_none())
        b.set_copy(shape, "web_read_only", read_only);
    b.set(shape, "entity_singular", values::py_str(b.tables(),
        values::member(b.tables(), junction, "name_singular"), ""));
    b.set(shape, "entity_plural", name);
    b.set(shape, "entity_plural_short", name);
    b.set(shape, "current_state", true);

    std::vector<value_id> sides;
    for (const auto key : {"left", "right"})
        sides.push_back(values::member(b.tables(), junction, key));
    const auto cpp_type_of = [&](value_id o) {
        return values::truthy(b.tables(), o, "cpp_type")
            ? std::string(values::str(b.tables(), o, "cpp_type"))
            : std::string("std::string");
    };
    const auto key_columns = b.list(b.object(shape, "primary_key"), "columns");
    for (const auto side : sides) {
        const auto o = b.append_object(key_columns);
        b.set(o, "column", std::string(values::str(b.tables(), side, "column")));
        b.set(o, "cpp_type", cpp_type_of(side));
        b.set(o, "is_uuid", values::str(b.tables(), side, "type") == "uuid");
    }
    const auto columns = b.list(shape, "columns");
    for (const auto side : sides) {
        const auto o = b.append_object(columns);
        b.set(o, "column", std::string(values::str(b.tables(), side, "column")));
        b.set(o, "cpp_type", cpp_type_of(side));
    }
    for (const auto c : values::items(b.tables(), junction, "columns")) {
        const auto o = b.append_object(columns);
        b.set(o, "column", wire::column_name(b.tables(), c));
        b.set(o, "cpp_type", cpp_type_of(c));
    }
    const auto requests = b.list(shape, "extra_list_requests");
    for (const auto side : sides) {
        if (!values::truthy(b.tables(), side, "list_by"))
            continue;
        const std::string column(values::str(b.tables(), side, "column"));
        const auto o = b.append_object(requests);
        b.set(o, "filter_column", column);
        b.set(o, "nats_suffix", "list_by_" + column);
    }
    return shape;
}

/*
 * The shape the shell and HTTP route templates read under domain_entity:
 * junction_entity_shape with the junction's messages and operations, its
 * write record, the surface plans and the names those templates spell.
 */
value_id junction_surface_shape(builder& b, value_id junction,
    const types::entity_registry& registry) {
    const auto shape = junction_entity_shape(b, junction);
    b.set_copy(shape, "messages", values::member(b.tables(), junction,
        "messages"));
    b.set_copy(shape, "has_generated_wire_codec", values::member(b.tables(),
        junction, "has_generated_wire_codec"));
    const auto fields = wire::write_record_for(b.tables(), shape);
    const auto list = b.list(shape, "write_fields");
    for (const auto& f : fields)
        wire::append_field(b, list, f, registry.custom_types);
    for (const auto c : values::items(b.tables(), junction)) {
        const auto key = values::key_of(b.tables(), c);
        if (key == "operations" || key.ends_with("_operations"))
            b.set_copy(shape, std::string(key), c);
    }
    // The plans read filter_members, which the Python derives as it reads
    // them rather than keeping on the shape.
    write_filter_members(b, shape);
    derive_surface_plans(b, shape);
    b.remove(shape, "filter_members");
    const auto cpp = values::member(b.tables(), junction, "cpp");
    const auto sub = values::truthy(b.tables(), cpp, "subcomponent")
        ? values::member(b.tables(), cpp, "subcomponent")
        : values::member(b.tables(), junction, "subcomponent");
    if (values::truthy(b.tables(), sub))
        b.set_copy(shape, "subcomponent", sub);
    else
        b.set(shape, "subcomponent", "api");
    const auto upper = values::member(b.tables(), junction, "entity_upper");
    if (values::truthy(b.tables(), upper))
        b.set_copy(shape, "entity_singular_upper", upper);
    else
        b.set(shape, "entity_singular_upper", text::upper(
            values::str(b.tables(), shape, "entity_singular")));
    const auto words = values::member(b.tables(), junction,
        "entity_plural_words");
    if (values::truthy(b.tables(), words))
        b.set_copy(shape, "entity_plural_words", words);
    else
        b.set_copy(shape, "entity_plural_words",
            values::member(b.tables(), shape, "entity_plural"));
    return shape;
}

bool has_prefix(std::string_view name,
    std::initializer_list<std::string_view> prefixes) {
    return std::ranges::any_of(prefixes,
        [&](std::string_view p) { return name.starts_with(p); });
}

bool has_suffix(std::string_view name,
    std::initializer_list<std::string_view> suffixes) {
    return std::ranges::any_of(suffixes,
        [&](std::string_view s) { return name.ends_with(s); });
}

/*
 * junction_protocol_messages: the entity set of the junction's shape, with
 * no announcement record unless it states :events: true, and no write
 * operation or write-only record when its wire takes no writes.
 */
void derive_junction_messages(builder& b, value_id junction,
    const types::entity_registry& registry) {
    const auto shape = junction_entity_shape(b, junction);
    ops::derive_messages(b, shape, registry, {});
    const auto& t = b.tables();
    const bool announces = values::truthy(t, junction, "events");
    const auto stated = values::member(t, junction, "wire_write_enabled");
    const bool writes = stated.is_none()
        ? !(values::truthy(t, junction, "read_only") ||
            values::truthy(t, junction, "client_read_only"))
        : values::truthy(t, stated);
    const auto list = b.list(junction, "messages");
    for (const auto m : values::items(b.tables(), shape, "messages")) {
        const auto name = values::str(b.tables(), m, "name");
        if (!announces && name.ends_with("_event"))
            continue;
        if (!writes && (has_prefix(name, {"put_", "delete_"}) ||
                has_suffix(name, {"_write", "_change", "_removal"})))
            continue;
        b.append_copy(list, m);
    }
}

void derive_junction_write_fields(builder& b, value_id junction,
    const types::entity_registry& registry) {
    const auto shape = junction_entity_shape(b, junction);
    const auto fields = wire::write_record_for(b.tables(), shape);
    const auto list = b.list(junction, "write_fields");
    for (const auto& f : fields)
        wire::append_field(b, list, f, registry.custom_types);
}

void derive_junction_events(builder& b, value_id junction) {
    const auto& t = b.tables();
    const auto prefix = std::string(values::str(t, junction, "component")) +
        ".v1.events." + std::string(values::str(t, junction, "name"));
    b.set(junction, "event_prefix", prefix);
    const auto events = b.list(junction, "events");
    for (const auto action : {"created", "updated", "deleted"}) {
        const auto e = b.append_object(events);
        b.set(e, "action", action);
        b.set(e, "subject", prefix + "." + action);
    }
}

void describe(builder& b, value_id junction) {
    const auto& t = b.tables();
    if (values::member(t, junction, "description").is_none())
        return;
    const std::string description(values::str(t, junction, "description"));
    b.set(junction, "description_formatted", text::comment_block(description));
    const auto lines = b.list(junction, "description_lines");
    for (const auto line : text::split(description, '\n'))
        b.append(lines, std::string(line));
}

void derive_junction_names(builder& b, value_id junction) {
    const auto& t = b.tables();
    if (!values::member(t, junction, "component").is_none()) {
        b.set(junction, "component_upper",
            text::upper(values::str(t, junction, "component")));
        set_component_path_vars(b, junction);
        for (const auto key : {"component_include", "component_core",
                 "component_service", "cache_component"})
            b.set(junction, std::string(key) + "_upper", text::upper_snake(
                values::str(b.tables(), junction, key)));
        for (const auto key : {"cache_subcomponent", "generator_facet_name"})
            b.set(junction, std::string(key) + "_upper",
                text::upper(values::str(b.tables(), junction, key)));
    }
    if (!values::member(b.tables(), junction, "name_singular").is_none()) {
        const std::string singular(values::str(b.tables(), junction,
            "name_singular"));
        b.set(junction, "name_singular_upper", text::upper(singular));
        if (values::member(b.tables(), junction, "name_singular_words")
                .is_none())
            b.set(junction, "name_singular_words",
                std::string(text::split(singular, '_').back()));
    }
    if (!values::member(b.tables(), junction, "name").is_none())
        b.set(junction, "name_upper",
            text::upper(values::str(b.tables(), junction, "name")));
    if (!values::member(b.tables(), junction, "name_title").is_none())
        b.set(junction, "name_title_lower",
            text::lower(values::str(b.tables(), junction, "name_title")));
}

void derive_junction_table_display(builder& b, value_id junction) {
    const auto& t = b.tables();
    const auto cpp = values::member(t, junction, "cpp");
    if (cpp.is_none())
        return;
    table_display_types types;
    for (const auto key : {"left", "right"}) {
        const auto side = values::member(t, junction, key);
        if (values::truthy(t, side, "is_uuid"))
            types.uuid.emplace(values::str(t, side, "column"));
    }
    for (const auto c : values::items(t, junction, "columns")) {
        const std::string name(values::str(t, c, "name"));
        const auto cpp_type = values::strip(values::str(t, c, "cpp_type"));
        const bool optional_uuid = values::truthy(t, c, "is_optional_uuid");
        if (values::truthy(t, c, "is_uuid") || optional_uuid)
            types.uuid.insert(name);
        if (cpp_type.starts_with("std::optional<") && !optional_uuid)
            types.optional.insert(name);
        if (cpp_type == "bool")
            types.boolean.insert(name);
    }
    prepare_table_display(b, cpp, types, {});
}

void derive_junction_tenant_scope(builder& b, value_id junction) {
    const bool visible = values::truthy(b.tables(), junction,
        "system_tenant_visible");
    b.set(junction, "system_tenant_visible", visible);
    if (visible) {
        b.set(junction, "tenant_where",
            "(\"tenant_id\"_c == tid || \"tenant_id\"_c == sys)");
        b.set(junction, "sys_decl", "\n    static const std::string sys("
            "ores::database::service::tenant_context::system_tenant_id);");
    } else {
        b.set(junction, "tenant_where", "\"tenant_id\"_c == tid");
        b.set(junction, "sys_decl", "");
    }
}

}

void flag_junction_columns(builder& b, value_id junction) {
    flag_columns(b, junction);
    flag_sides(b, junction);
}

junction_contexts junction_context(builder& b, value_id library,
    value_id model, std::string_view model_key,
    const types::entity_registry& registry, const types::entity_site& site) {
    const auto [context, junction] = base_context(b, library, model,
        model_key, "junction");
    if (junction.is_none())
        return {context, value_id::none()};
    mark_last_item(b, values::member(b.tables(), junction, "artefact_columns"));
    format_columns(b, junction);
    flag_junction_columns(b, junction);
    if (values::truthy(b.tables(), junction, "history"))
        derive_history(b, junction);
    if (values::truthy(b.tables(), junction, "events"))
        derive_announcements(b, junction);
    derive_include_gates(b, junction);
    derive_member_ts_types(b, junction, registry);
    derive_junction_messages(b, junction, registry);
    derive_codec_includes(b, junction, registry, site);
    b.set_integer(junction, "wire_left_out", 0);
    derive_message_struct_names(b, junction);
    derive_junction_write_fields(b, junction, registry);
    derive_operations(b, junction);
    const bool derived = !site.protocol_owned;
    b.set(junction, "protocol_derived", derived);
    b.set(junction, "service_uses_cstdint", derived &&
        (values::truthy(b.tables(), junction, "put_operations") ||
         values::truthy(b.tables(), junction, "delete_operations")));
    derive_junction_events(b, junction);
    const auto surface = junction_surface_shape(b, junction, registry);
    describe(b, junction);
    derive_junction_names(b, junction);
    derive_junction_table_display(b, junction);
    copy_repository_fields(b, junction);
    derive_junction_tenant_scope(b, junction);
    if (values::truthy(b.tables(), junction, "history"))
        copy_history_sides(b, junction);
    b.set_copy(context, "junction", junction);
    return {context, surface};
}

}

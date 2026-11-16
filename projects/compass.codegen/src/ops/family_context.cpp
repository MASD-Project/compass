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
#include <map>
#include <set>
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

using prefix_map = std::map<std::string, std::string, std::less<>>;

/*
 * _group_prefixes: the member prefix that carries each column of a
 * grouped model.
 */
prefix_map group_prefixes(const model_tables& t, value_id model) {
    prefix_map out;
    for (const auto f : values::items(t, model, "domain_group_fields")) {
        const auto name = values::str(t, f, "name");
        const auto dot = name.find('.');
        if (dot != std::string_view::npos && dot + 1 < name.size())
            out[std::string(name.substr(dot + 1))] =
                std::string(name.substr(0, dot)) + ".";
    }
    return out;
}

std::string prefix_of(const prefix_map& prefixes, std::string_view name) {
    const auto it = prefixes.find(name);
    return it == prefixes.end() ? std::string() : it->second;
}

bool is_numeric(std::string_view cpp_type) {
    constexpr std::string_view numeric[] = {"int", "long", "long long",
        "short", "double", "float", "std::int16_t", "std::int32_t",
        "std::int64_t", "std::uint16_t", "std::uint32_t", "std::uint64_t",
        "std::size_t"};
    return std::ranges::find(numeric, cpp_type) != std::end(numeric);
}

/*
 * The type and C++ type a key column is converted by, as the raw model
 * states them.
 */
struct column_type {
    std::string type;
    std::string cpp_type;
};

column_type type_of(const model_tables& t, value_id column) {
    return {std::string(values::str(t, column, "type")),
        std::string(values::str(t, column, "cpp_type"))};
}

/*
 * _family_text_expr: @p expression in the text form a key parameter
 * takes it in.
 */
std::string text_expr(const std::string& expression, const column_type& c) {
    if (c.type == "uuid" || c.cpp_type.contains("boost::uuids::uuid"))
        return "boost::uuids::to_string(" + expression + ")";
    if (c.type == "timestamp" || c.type == "timestamptz" ||
        c.type == "timestamp with time zone" ||
        c.cpp_type.contains("time_point"))
        return "ores::platform::time::datetime::to_db_string(" + expression +
            ")";
    if (is_numeric(c.cpp_type))
        return "std::to_string(" + expression + ")";
    return expression;
}

/*
 * The write-record column named @p name, or none.
 */
value_id write_column(const model_tables& t, value_id model,
    std::string_view name) {
    for (const auto c : wire::write_record_columns(t, model)) {
        if (wire::column_name(t, c) == name)
            return c;
    }
    return value_id::none();
}

/*
 * _declared_columns: whether @p model declares the column @p name in its
 * columns, natural keys or primary key columns.
 */
bool declares_column(const model_tables& t, value_id model,
    std::string_view name) {
    std::vector<value_id> entries = values::items(t, model, "columns");
    std::ranges::copy(values::items(t, model, "natural_keys"),
        std::back_inserter(entries));
    std::ranges::copy(values::items(t, values::member(t, model,
        "primary_key"), "columns"), std::back_inserter(entries));
    return std::ranges::any_of(entries, [&](value_id e) {
        const auto column = values::truthy(t, e, "column")
            ? values::str(t, e, "column") : values::str(t, e, "name");
        return column == name;
    });
}

/*
 * The member prefix of each column, primary key and natural key of a
 * grouped model that states none.
 */
void stamp_group_prefixes(builder& b, value_id model,
    const prefix_map& prefixes) {
    const auto& t = b.tables();
    const auto pk = values::member(t, model, "primary_key");
    std::vector<value_id> entries = values::items(t, model, "columns");
    if (!pk.is_none())
        entries.push_back(pk);
    std::ranges::copy(values::items(t, pk, "columns"),
        std::back_inserter(entries));
    std::ranges::copy(values::items(t, model, "natural_keys"),
        std::back_inserter(entries));
    for (const auto e : entries) {
        if (!values::truthy(b.tables(), e, "group_prefix"))
            b.set(e, "group_prefix", prefix_of(prefixes,
                wire::column_name(b.tables(), e)));
    }
}

/*
 * The facts the view states about a row's primary key column.
 */
struct key_column {
    std::string column;
    std::string member;
    std::string cpp_type;
    bool is_uuid = false;
    column_type type;
};

/*
 * _family_entity_view of the entity @p model, a copy the view may change,
 * written onto @p view.
 */
key_column write_entity_view(builder& b, value_id view, value_id model,
    std::string_view role, std::string_view owner,
    const types::entity_registry& registry) {
    const auto& t = b.tables();
    const auto prefixes = group_prefixes(t, model);
    stamp_group_prefixes(b, model, prefixes);
    const std::string entity(values::str(b.tables(), model, "entity_singular"));
    const std::string component(values::str(b.tables(), model, "component"));
    const auto sql = values::member(b.tables(), model, "sql");
    const bool current_state = values::truthy(b.tables(), sql, "current_state");
    const bool audit = !values::truthy(b.tables(), sql, "no_audit_columns") &&
        !current_state;

    const auto pk_columns = values::items(b.tables(),
        values::member(b.tables(), model, "primary_key"), "columns");
    if (pk_columns.size() != 1)
        throw std::runtime_error(std::string(owner) + ": the " +
            std::string(role) + " '" + entity + "' has " +
            std::to_string(pk_columns.size()) + " primary-key columns; the "
            "composite family service addresses a row by one key column");
    const auto pk = pk_columns.front();
    key_column key;
    key.column = wire::column_name(b.tables(), pk);
    key.type = type_of(b.tables(), pk);
    key.cpp_type = !key.type.cpp_type.empty() ? key.type.cpp_type :
        key.type.type == "uuid" ? "boost::uuids::uuid" : "std::string";
    key.is_uuid = key.type.type == "uuid" ||
        key.cpp_type.contains("boost::uuids::uuid");
    const auto stated_prefix = values::str(b.tables(), pk, "group_prefix");
    key.member = (!stated_prefix.empty() ? std::string(stated_prefix) :
        prefix_of(prefixes, key.column)) + key.column;

    const auto key_fields = wire::key_record_fields(b.tables(), model);
    const auto declared = wire::declared_key_field(b.tables(), model);
    const bool by_primary = wire::key_is_primary(b.tables(), model) ||
        declared.empty();
    std::string read_method = "read_latest";
    std::string read_key_expr;
    std::string key_member = key.member;
    if (by_primary) {
        const auto name = key_fields.empty() ? key.column :
            key_fields.front().name;
        read_key_expr = text_expr("key." + name, key.type);
    } else {
        const auto column = write_column(b.tables(), model, declared);
        const auto type = column.is_none()
            ? column_type{"text", "std::string"}
            : type_of(b.tables(), column);
        read_method = "read_latest_by_" + declared;
        read_key_expr = text_expr("key." + declared, type);
        const auto column_prefix = values::str(b.tables(), column,
            "group_prefix");
        key_member = (!column_prefix.empty() ? std::string(column_prefix) :
            prefix_of(prefixes, declared)) + declared;
    }
    derive_write_fields(b, model, registry);

    const auto plural = values::member(b.tables(), model, "entity_plural");
    b.set(view, "entity", entity);
    if (plural.is_none())
        b.set(view, "entity_plural", entity + "s");
    else
        b.set_copy(view, "entity_plural", plural);
    b.set(view, "domain", "ores::" + component + "::domain::" + entity);
    b.set(view, "key_record", entity + "_key");
    b.set(view, "write_record", entity + "_write");
    b.set(view, "change_record", entity + "_change");
    const auto key_list = b.list(view, "key_fields");
    for (const auto& f : key_fields)
        wire::append_field(b, key_list, f, registry.custom_types);
    if (key_fields.empty()) {
        const auto o = b.object(view, "key_field");
        b.set(o, "name", key.column);
        b.set(o, "cpp_type", key.cpp_type);
    } else {
        b.set_copy(view, "key_field",
            values::items(b.tables(), key_list).front());
    }
    b.set(view, "key_is_primary", by_primary);
    b.set(view, "declared_key", declared.empty() ? key.column : declared);
    b.set(view, "read_method", read_method);
    b.set(view, "read_key_expr", read_key_expr);
    b.set(view, "key_member", key_member);
    b.set(view, "pk_column", key.column);
    b.set(view, "pk_member", key.member);
    b.set(view, "pk_cpp_type", key.cpp_type);
    b.set(view, "pk_is_uuid", key.is_uuid);
    b.set(view, "pk_text_expr", text_expr("row." + key.member, key.type));
    b.set_copy(view, "write_fields",
        values::member(b.tables(), model, "write_fields"));
    b.set(view, "has_audit_columns", audit);
    b.set(view, "has_party_id", declares_column(b.tables(), model, "party_id"));
    b.set(view, "party_member", prefix_of(prefixes, "party_id") + "party_id");

    const auto grouped = b.list(view, "grouped_members");
    for (const auto g : values::items(b.tables(), model, "domain_groups"))
        b.append_copy(grouped, values::member(b.tables(), g, "member"));

    constexpr std::string_view stamped[] = {"tenant_id", "party_id",
        "modified_by", "performed_by", "change_reason_code"};
    std::set<std::string, std::less<>> stamped_members;
    for (const auto f : values::items(b.tables(), model,
             "domain_group_fields")) {
        const auto name = values::str(b.tables(), f, "name");
        const auto dot = name.find('.');
        if (dot != std::string_view::npos && dot + 1 < name.size() &&
            std::ranges::find(stamped, name.substr(dot + 1)) !=
                std::end(stamped))
            stamped_members.emplace(name.substr(0, dot));
    }
    const auto stamps = b.list(view, "stamp_members");
    for (const auto g : values::items(b.tables(), model, "domain_groups")) {
        const std::string member(values::str(b.tables(), g, "member"));
        if (stamped_members.contains(member))
            b.set(b.append_object(stamps), "member", member);
    }
    const auto version = audit ? std::string("version") : key.column;
    b.set(view, "version_member", prefix_of(prefixes, version) + version);
    b.set(view, "commentary_member", audit ? prefix_of(prefixes,
        "change_commentary") + "change_commentary" : std::string());
    return key;
}

/*
 * A copy of the loaded entity @p model the view may change.
 */
value_id scratch_copy(builder& b, value_id model) {
    const auto holder = b.detached_object();
    b.set_copy(holder, "model", model);
    return b.find(holder, "model");
}

/*
 * _family_service_view on @p family: the central row's view, each
 * part's view with how it reaches the central row, the operations with
 * their permission codes, and the words the service spells.
 */
void derive_service_view(builder& b, value_id family,
    const family_members& members, const types::entity_registry& registry,
    std::string_view site_path) {
    const auto& t = b.tables();
    const std::string owner(values::truthy(t, family, "entity_singular")
        ? values::str(t, family, "entity_singular")
        : site_path.substr(site_path.rfind('/') + 1));
    const std::string central_entity(values::str(t, family, "central_entity"));
    const auto model_of = [&](const std::string& entity) {
        const auto it = members.find(entity);
        if (it == members.end())
            throw std::runtime_error(owner + ": names the entity '" + entity +
                "', but no ores.codegen.entity model beside the family "
                "declares it");
        return scratch_copy(b, it->second);
    };

    const auto central_view = b.object(family, "central");
    const auto central = write_entity_view(b, central_view,
        model_of(central_entity), "central row", owner, registry);
    const auto central_party = std::string(values::str(b.tables(),
        central_view, "party_member"));
    const bool central_has_party = values::truthy(b.tables(), central_view,
        "has_party_id");

    for (const auto part : values::items(b.tables(), family, "parts")) {
        const std::string entity(values::str(b.tables(), part, "entity"));
        const std::string name(values::str(b.tables(), part, "part"));
        const auto model = model_of(entity);
        write_entity_view(b, part, model, "part '" + name + "'", owner,
            registry);
        b.set(part, "part", name);
        value_id foreign_key = value_id::none();
        for (const auto fk : values::items(b.tables(), model, "foreign_keys")) {
            if (values::str(b.tables(), fk, "parent_entity") == central_entity) {
                foreign_key = fk;
                break;
            }
        }
        if (foreign_key.is_none())
            throw std::runtime_error(owner + ": the part '" + name + "' (" +
                entity + ") has no foreign key whose :parent_entity: is '" +
                central_entity + "'");
        const std::string fk_column(values::str(b.tables(), foreign_key,
            "column"));
        const auto fk_dict = write_column(b.tables(), model, fk_column);
        const auto fk_type = fk_dict.is_none()
            ? column_type{"uuid", "boost::uuids::uuid"}
            : type_of(b.tables(), fk_dict);
        const bool fk_is_uuid = fk_type.type == "uuid" ||
            fk_type.cpp_type.contains("boost::uuids::uuid");
        if (fk_is_uuid && !central.is_uuid)
            throw std::runtime_error(owner + ": the part '" + name +
                "' foreign key '" + fk_column + "' is a uuid but the central "
                "key is not; the family service cannot join them");
        const auto read_expr = central.is_uuid
            ? "boost::uuids::to_string(central." + central.member + ")"
            : is_numeric(central.cpp_type)
            ? "std::to_string(central." + central.member + ")"
            : "central." + central.member;
        const auto fk_prefix = values::str(b.tables(), fk_dict, "group_prefix");
        b.set(part, "fk_column", fk_column);
        b.set(part, "fk_member", (!fk_prefix.empty() ? std::string(fk_prefix)
            : prefix_of(group_prefixes(b.tables(), model), fk_column)) +
            fk_column);
        b.set(part, "fk_is_uuid", fk_is_uuid);
        b.set(part, "fk_list_by_uuid", values::truthy(b.tables(), foreign_key,
            "list_by_uuid") && fk_is_uuid);
        b.set(part, "fk_read_expr", read_expr);
        b.set(part, "fk_assign_expr", fk_is_uuid ?
            "central." + central.member : read_expr);
        b.set(part, "copy_party_id", central_has_party &&
            values::truthy(b.tables(), part, "has_party_id"));
        b.set(part, "central_party_member", central_party);
    }

    derive_operations(b, family);
    const auto prefix = std::string(values::str(b.tables(), family,
        "component")) + "::" + std::string(values::str(b.tables(),
        central_view, "entity_plural"));
    // The verb lists hold the very operations the Python codes, so each
    // copy of an operation is coded.
    for (const auto list : values::items(b.tables(), family)) {
        if (!values::key_of(b.tables(), list).ends_with("operations"))
            continue;
        for (const auto op : values::items(b.tables(), list))
            b.set(op, "permission_code", prefix + ":" +
                std::string(values::str(b.tables(), op, "permission")));
    }
    b.set(family, "permission_prefix", prefix);
    for (const auto key : {"entity_singular", "entity_plural"}) {
        std::string words(values::str(b.tables(), family, key));
        std::ranges::replace(words, '_', ' ');
        b.set(family, std::string(key) + "_words", words);
    }
}

}

family_contexts family_context(builder& b, value_id library, value_id model,
    std::string_view model_key, const types::entity_registry& registry,
    const types::entity_site& site, const family_members& members) {
    const auto [context, family] = base_context(b, library, model, model_key,
        "family");
    if (family.is_none())
        return {context, value_id::none()};
    set_component_path_vars(b, family);
    for (const auto key : {"component", "component_include", "component_core",
             "component_service"})
        b.set(family, std::string(key) + "_upper", text::upper_snake(
            values::str(b.tables(), family, key)));
    if (!values::member(b.tables(), family, "entity_singular").is_none())
        b.set(family, "entity_singular_upper", text::upper(
            values::str(b.tables(), family, "entity_singular")));

    const auto service = b.detached_object();
    b.set_copy(service, model_key, values::member(b.tables(), context,
        model_key));
    const auto serviced = values::member(b.tables(),
        values::member(b.tables(), service, model_key), "family");
    derive_service_view(b, serviced, members, registry, site.path);
    for (const auto f : {family, serviced})
        project_wire_codec(b, f, registry, site);
    b.set_copy(context, "family", family);
    for (const auto c : values::items(b.tables(), context)) {
        const auto key = values::key_of(b.tables(), c);
        if (key != model_key)
            b.set_copy(service, std::string(key), c);
    }
    b.set_copy(service, "family", serviced);
    return {context, service};
}

}

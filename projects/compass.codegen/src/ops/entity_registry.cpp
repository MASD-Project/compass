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
#include "compass.codegen/ops/entity_registry.hpp"

#include <algorithm>
#include <vector>
#include "compass.codegen/ops/values.hpp"
#include "entity_text.hpp"

namespace compass::codegen::ops {

using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
        c == '\v';
}

bool is_lower_or_digit(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

/*
 * ^#\+entity_singular:\s*(\S+), multi-line.
 */
std::string first_entity_singular(std::string_view text) {
    constexpr std::string_view marker = "#+entity_singular:";
    for (auto at = text.find(marker); at != std::string_view::npos;
         at = text.find(marker, at + 1)) {
        if (at != 0 && text[at - 1] != '\n')
            continue;
        auto p = at + marker.size();
        while (p < text.size() && is_space(text[p]))
            ++p;
        auto end = p;
        while (end < text.size() && !is_space(text[end]))
            ++end;
        if (end > p)
            return std::string(text.substr(p, end - p));
    }
    return {};
}

/*
 * :tablename:\s+([a-z][a-z0-9_]*_tbl), first match: the longest run of
 * name characters that ends in _tbl.
 */
std::string first_tablename(std::string_view text) {
    constexpr std::string_view marker = ":tablename:";
    constexpr std::string_view suffix = "_tbl";
    for (auto at = text.find(marker); at != std::string_view::npos;
         at = text.find(marker, at + 1)) {
        auto p = at + marker.size();
        const auto spaced = p;
        while (p < text.size() && is_space(text[p]))
            ++p;
        if (p == spaced || p == text.size() || text[p] < 'a' || text[p] > 'z')
            continue;
        auto end = p + 1;
        while (end < text.size() && is_lower_or_digit(text[end]))
            ++end;
        for (auto stop = end; stop >= p + 1 + suffix.size(); --stop) {
            if (text.substr(stop - suffix.size(), suffix.size()) == suffix)
                return std::string(text.substr(p, stop - p));
        }
    }
    return {};
}

std::string name_or_column(const model_tables& t, value_id c) {
    const auto name = values::member(t, c, "name");
    return values::truthy(t, name) ? values::py_str(t, name)
                                   : values::py_str(t, values::member(t, c, "column"));
}

/*
 * The columns, the primary key's columns and the natural keys, in the
 * order the Python's three-source loops read them.
 */
std::vector<value_id> declared_columns(const model_tables& t, value_id de) {
    auto out = values::items(t, de, "columns");
    const auto pk = values::items(t, values::member(t, de, "primary_key"),
        "columns");
    out.insert(out.end(), pk.begin(), pk.end());
    const auto nks = values::items(t, de, "natural_keys");
    out.insert(out.end(), nks.begin(), nks.end());
    return out;
}

std::map<std::string, std::string, std::less<>> column_member_prefixes(
    const model_tables& t, value_id de) {
    std::map<std::string, std::string, std::less<>> prefixes;
    for (const auto field : values::items(t, de, "domain_group_fields")) {
        const auto name = values::py_str(t, values::member(t, field, "name"));
        const auto dot = name.find('.');
        if (dot == std::string::npos || dot == 0 || dot + 1 == name.size())
            continue;
        prefixes[name.substr(dot + 1)] = name.substr(0, dot + 1);
    }
    const bool has_identity = values::truthy(t, de, "domain_identity_group");
    for (const auto c : declared_columns(t, de)) {
        const auto name = name_or_column(t, c);
        if (name.empty() || prefixes.contains(name))
            continue;
        if (has_identity && values::str(t, c, "group") == "identity")
            prefixes[name] = "identity.";
    }
    if (values::truthy(t, de, "domain_audit_group")) {
        for (const auto* name : {"change_reason_code", "change_commentary",
                 "modified_by", "performed_by", "recorded_at"})
            prefixes.try_emplace(name, "audit.");
    }
    return prefixes;
}

bool is_plain_identifier(std::string_view s) {
    if (s.empty() || !((s[0] >= 'a' && s[0] <= 'z') || s[0] == '_'))
        return false;
    return std::ranges::all_of(s, is_lower_or_digit);
}

std::vector<std::set<std::string>> unique_keys_of(const model_tables& t,
    value_id de) {
    std::set<std::string> pk;
    for (const auto c : values::items(t, values::member(t, de, "primary_key"),
             "columns"))
        pk.insert(values::py_str(t, values::member(t, c, "column"), "None"));
    if (immutable_tenant_key_of(t, de))
        pk.insert("tenant_id");
    std::vector<std::set<std::string>> keys;
    if (!pk.empty())
        keys.push_back(pk);
    for (const auto index : values::items(t, de, "indexes")) {
        if (!values::truthy(t, index, "unique") ||
            values::truthy(t, index, "current_only") ||
            values::truthy(t, index, "where_extra"))
            continue;
        std::set<std::string> columns;
        bool plain = true;
        for (const auto part : text::split(values::str(t, index, "columns"),
                 ',')) {
            const auto column = values::strip(part);
            plain = plain && is_plain_identifier(column);
            columns.emplace(column);
        }
        if (plain)
            keys.push_back(columns);
    }
    return keys;
}

}

std::optional<entity_table_claim> entity_table_claim_of(
    std::string_view org_text) {
    auto singular = first_entity_singular(org_text);
    if (singular.empty())
        return std::nullopt;
    auto table = first_tablename(org_text);
    if (table.empty())
        return std::nullopt;
    return entity_table_claim{std::move(table), std::move(singular)};
}

std::set<std::string, std::less<>> column_names_of(const model_tables& t,
    value_id entity) {
    std::set<std::string, std::less<>> names;
    for (const auto c : declared_columns(t, entity)) {
        auto name = name_or_column(t, c);
        if (!name.empty())
            names.insert(std::move(name));
    }
    return names;
}

bool immutable_tenant_key_of(const model_tables& t, value_id entity) {
    const auto sql = values::member(t, entity, "sql");
    return values::truthy(t, sql, "immutable") &&
        values::truthy(t, entity, "has_tenant_id") &&
        !values::truthy(t, sql, "system_scope") &&
        !values::truthy(t, sql, "nullable_tenant_id");
}

types::parent_entity parent_entity_of(const model_tables& t, value_id model) {
    types::parent_entity p;
    const auto de = values::member(t, model, "domain_entity");
    const auto sql = values::member(t, de, "sql");
    p.column_prefixes = column_member_prefixes(t, de);
    p.entity_singular = values::py_str(t,
        values::member(t, de, "entity_singular"));
    p.generator_facet_name = values::truthy(t, de, "generator_facet_name")
        ? values::py_str(t, values::member(t, de, "generator_facet_name"))
        : std::string();
    p.component = values::py_str(t, values::member(t, de, "component"),
        "None");
    const auto audit = p.column_prefixes.find("change_reason_code");
    p.has_audit_group = values::truthy(t, de, "domain_audit_group") ||
        (audit != p.column_prefixes.end() && audit->second == "audit.");
    p.has_identity_group = values::truthy(t, de, "domain_identity_group") ||
        std::ranges::any_of(p.column_prefixes,
            [](const auto& e) { return e.second == "identity."; });
    p.seed_country_sentinel = values::truthy(t, de, "seed_country_sentinel");
    p.seed_currency = values::truthy(t, de, "seed_currency");
    p.no_audit_columns = values::truthy(t, sql, "no_audit_columns");
    for (const auto fk : values::items(t, de, "foreign_keys")) {
        if (values::truthy(t, fk, "nullable"))
            continue;
        p.mandatory_fks.push_back({
            values::py_str(t, values::member(t, fk, "column"), "None"),
            values::py_str(t, values::member(t, fk, "table"), "None"),
            values::py_str(t, values::member(t, fk, "target_column")),
            values::truthy(t, fk, "use_system_tenant")});
    }
    p.immutable = values::truthy(t, sql, "immutable");
    p.immutable_tenant_key = immutable_tenant_key_of(t, de);
    p.has_tenant_id = values::truthy(t, de, "has_tenant_id");
    p.party_isolated = values::truthy(t, sql, "rls_party_isolation");
    p.current_state = values::truthy(t, sql, "current_state");
    p.unique_keys = unique_keys_of(t, de);
    p.column_names = column_names_of(t, de);
    return p;
}

const types::parent_entity* parent_of(const types::entity_registry& registry,
    std::string_view table) {
    const auto it = registry.tables.find(table);
    if (it == registry.tables.end() || !it->second.parent)
        return nullptr;
    return &*it->second.parent;
}

}

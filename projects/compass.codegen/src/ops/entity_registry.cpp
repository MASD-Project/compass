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

/*
 * ^#\+<key>:\s*(\S+)\s*$, multi-line: the token of the first line that
 * states the key and one token after it, the key matched ignoring case
 * when @p any_case.
 */
std::string single_token_keyword(std::string_view text, std::string_view key,
    bool any_case = false) {
    const auto head = "#+" + std::string(key) + ":";
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        const auto line = text.substr(start, end - start);
        start = end + 1;
        if (line.size() < head.size())
            continue;
        const auto prefix = line.substr(0, head.size());
        if (any_case ? text::lower(prefix) != head : prefix != head)
            continue;
        const auto rest = values::strip(line.substr(head.size()));
        if (!rest.empty() && std::ranges::none_of(rest, is_space))
            return std::string(rest);
    }
    return {};
}

}

void add_directory_model(types::model_directory& d,
    std::string_view org_text) {
    const auto type = single_token_keyword(org_text, "type", true);
    if (type == "ores.codegen.entity") {
        auto name = single_token_keyword(org_text, "entity_singular");
        if (!name.empty()) {
            d.entity_singulars.insert(name);
            d.member_names.insert(std::move(name));
        }
    } else if (type == "ores.codegen.junction") {
        auto name = single_token_keyword(org_text, "name_singular");
        if (!name.empty())
            d.member_names.insert(std::move(name));
    } else if (type == "ores.codegen.field_group") {
        auto entity = single_token_keyword(org_text, "entity_singular");
        auto name = single_token_keyword(org_text, "name_singular");
        if (!entity.empty() && !name.empty())
            d.group_names.insert(org_text.find("#+entity_singular:") <
                org_text.find("#+name_singular:") ? entity : name);
        else if (!entity.empty() || !name.empty())
            d.group_names.insert(entity.empty() ? name : entity);
    }
}

const types::model_directory& directory_of(
    const types::entity_registry& registry, std::string_view model_path) {
    static const types::model_directory none;
    const auto at = model_path.rfind('/');
    const auto it = registry.directories.find(at == std::string_view::npos ?
        std::string_view() : model_path.substr(0, at));
    return it == registry.directories.end() ? none : it->second;
}

std::vector<types::trade_type_route> trade_type_routes_of(
    const compass::model::ops::outline& o) {
    namespace mops = compass::model::ops;
    std::vector<types::trade_type_route> out;
    std::vector<mops::node_id> pending{
        mops::section(o, mops::node_id{0}, "Trade types")};
    std::optional<std::uint32_t> table;
    while (!table && !pending.empty() && !pending.front().is_none()) {
        const auto n = pending.front();
        pending.erase(pending.begin());
        const auto tables = mops::tables_of(o, n);
        if (!tables.empty())
            table = tables.front();
        const auto kids = mops::children(o, n);
        pending.insert(pending.begin(), kids.begin(), kids.end());
    }
    if (!table || mops::row_count(o, *table) == 0)
        return out;
    const auto header = mops::first_row(o, *table);
    std::optional<std::uint32_t> code;
    std::optional<std::uint32_t> instrument;
    for (std::uint32_t i = 0; i < mops::cell_count(o, header); ++i) {
        const auto name = mops::cell(o, header, i);
        if (name == "code")
            code = i;
        else if (name == "instrument")
            instrument = i;
    }
    const auto at = [&](std::uint32_t row, std::optional<std::uint32_t> i) {
        return i && *i < mops::cell_count(o, row)
            ? std::string(values::strip(mops::cell(o, row, *i)))
            : std::string();
    };
    for (auto r = header + 1; r < header + mops::row_count(o, *table); ++r) {
        auto c = at(r, code);
        if (!c.empty())
            out.push_back({std::move(c), at(r, instrument)});
    }
    return out;
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

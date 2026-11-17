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
#include "compass.codegen/ops/facts.hpp"

#include <concepts>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include "compass.store/ops/blob_hash.hpp"

namespace compass::codegen::ops {

namespace {

/**
 * @brief The fields of each registry type, all of them: the binding fails
 * to compile when a field is added, so a new field cannot be left out of
 * the value of a fact.
 */
auto fields(const types::mandatory_foreign_key& v) {
    const auto& [column, table, target_column, use_system_tenant] = v;
    return std::tie(column, table, target_column, use_system_tenant);
}

auto fields(const types::parent_entity& v) {
    const auto& [entity_singular, generator_facet_name, component,
        has_audit_group, has_identity_group, seed_country_sentinel,
        seed_currency, no_audit_columns, immutable, immutable_tenant_key,
        has_tenant_id, party_isolated, current_state, column_prefixes,
        mandatory_fks, unique_keys, column_names] = v;
    return std::tie(entity_singular, generator_facet_name, component,
        has_audit_group, has_identity_group, seed_country_sentinel,
        seed_currency, no_audit_columns, immutable, immutable_tenant_key,
        has_tenant_id, party_isolated, current_state, column_prefixes,
        mandatory_fks, unique_keys, column_names);
}

auto fields(const types::table_owner& v) {
    const auto& [entity_singular, parent] = v;
    return std::tie(entity_singular, parent);
}

auto fields(const types::trade_type_route& v) {
    const auto& [code, instrument] = v;
    return std::tie(code, instrument);
}

auto fields(const types::model_directory& v) {
    const auto& [entity_singulars, member_names, group_names] = v;
    return std::tie(entity_singulars, member_names, group_names);
}

auto fields(const types::convention_family& v) {
    const auto& [key, entity, domain_type, subject_stem, party_scoped,
        write_mode, component, has_wire_codec] = v;
    return std::tie(key, entity, domain_type, subject_stem, party_scoped,
        write_mode, component, has_wire_codec);
}

template <typename T>
concept has_fields = requires(const T& v) { fields(v); };

// Only a bool itself is written as one: an integer or a pointer must not
// convert to it and lose its value.
template <std::same_as<bool> T> void put(std::string& out, T v) {
    out.push_back(v ? '\1' : '\0');
}

void put(std::string& out, const std::string& v) {
    const auto size = static_cast<std::uint32_t>(v.size());
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<char>((size >> shift) & 0xFF));
    out += v;
}

template <has_fields T> void put(std::string& out, const T& v);
template <typename T> void put(std::string& out, const std::optional<T>& v);
template <typename A, typename B>
void put(std::string& out, const std::pair<A, B>& v);
template <typename... T>
void put(std::string& out, const std::tuple<T...>& v);
template <typename T> void put(std::string& out, const std::vector<T>& v);
template <typename T, typename L>
void put(std::string& out, const std::set<T, L>& v);
template <typename K, typename V, typename L>
void put(std::string& out, const std::map<K, V, L>& v);

template <typename Range>
void put_range(std::string& out, const Range& v) {
    put(out, std::to_string(v.size()));
    for (const auto& e : v)
        put(out, e);
}

template <has_fields T> void put(std::string& out, const T& v) {
    std::apply([&](const auto&... field) { (put(out, field), ...); },
        fields(v));
}

template <typename T> void put(std::string& out, const std::optional<T>& v) {
    put(out, v.has_value());
    if (v)
        put(out, *v);
}

template <typename A, typename B>
void put(std::string& out, const std::pair<A, B>& v) {
    put(out, v.first);
    put(out, v.second);
}

template <typename... T>
void put(std::string& out, const std::tuple<T...>& v) {
    std::apply([&](const auto&... part) { (put(out, part), ...); }, v);
}

template <typename T> void put(std::string& out, const std::vector<T>& v) {
    put_range(out, v);
}

template <typename T, typename L>
void put(std::string& out, const std::set<T, L>& v) {
    put_range(out, v);
}

template <typename K, typename V, typename L>
void put(std::string& out, const std::map<K, V, L>& v) {
    put_range(out, v);
}

template <typename Container>
void publish(std::vector<fact>& out,
    const types::recorded<Container>& collection) {
    const auto& all = collection.unrecorded();
    std::string bytes;
    put(bytes, all);
    out.push_back(fact_of(std::string(collection.name()), bytes));
    if constexpr (requires { typename Container::mapped_type; }) {
        for (const auto& [key, value] : all) {
            bytes.clear();
            put(bytes, value);
            out.push_back(fact_of(types::fact_name(collection.name(), key),
                bytes));
        }
    } else if constexpr (requires { typename Container::key_type; }) {
        for (const auto& key : all)
            out.push_back(fact_of(types::fact_name(collection.name(), key),
                {}));
    }
}

}

fact fact_of(std::string name, std::string_view bytes) {
    return {std::move(name), {compass::store::ops::sha1_of(bytes)}};
}

void publish_facts(std::vector<fact>& out, const types::entity_registry& r) {
    publish(out, r.tables);
    publish(out, r.trade_type_routes);
    publish(out, r.custom_types);
    publish(out, r.economic_field_groups);
    publish(out, r.directories);
    publish(out, r.convention_families);
    publish(out, r.shell_units);
    publish(out, r.shell_local_units);
}

void publish_facts(std::vector<fact>& out,
    const types::checkout_registry& r) {
    publish(out, r.protocol_owners);
    publish(out, r.plural_owners);
    publish(out, r.shell_registrars);
}

}

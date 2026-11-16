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
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

bool is_uuid_column(const model_tables& t, value_id col) {
    return values::str(t, col, "type") == "uuid" ||
        values::str(t, col, "cpp_type").contains("boost::uuids::uuid");
}

/*
 * _derive_supply: who supplies the column's value, and the member path on
 * the domain struct. Returns whether the user supplies it.
 */
bool derive_column_supply(builder& b, value_id col,
    std::string_view fallback) {
    const auto& t = b.tables();
    const std::string supply(values::truthy(t, col, "supplied_by")
        ? values::str(t, col, "supplied_by") : fallback);
    std::string name(values::str(t, col, "name"));
    if (name.empty())
        name = values::str(t, col, "column");
    const auto access = std::string(values::str(t, col, "group_prefix")) + name;
    b.set(col, "is_user_supplied", supply == "user");
    b.set(col, "is_minted", supply == "minted");
    b.set(col, "is_session_party", supply == "session_party");
    b.set(col, "member_access", access);
    return supply == "user";
}

std::string_view key_supply(const model_tables& t, value_id col) {
    return is_uuid_column(t, col) ? "minted" : "user";
}

/*
 * The primary key and the members the shell units read for it: its
 * columns, or the key itself when it has none. An entity without a key
 * gets what Python's .get('primary_key', {}) gives it, an empty key that
 * no value holds, with the supply flags derived as the Python derives
 * them on it.
 */
struct primary_key {
    value_id key;
    std::vector<value_id> members;
};

primary_key primary_key_of(builder& b, value_id entity) {
    auto key = values::member(b.tables(), entity, "primary_key");
    if (key.is_none()) {
        key = b.detached_object();
        derive_column_supply(b, key, key_supply(b.tables(), key));
    }
    auto members = values::items(b.tables(), key, "columns");
    if (members.empty())
        members.push_back(key);
    return {key, std::move(members)};
}

std::string unwrapped_cpp_type(std::string_view cpp_type) {
    auto cpp = values::strip(cpp_type);
    constexpr std::string_view optional = "std::optional<";
    if (cpp.starts_with(optional) && cpp.ends_with('>'))
        cpp = values::strip(cpp.substr(optional.size(),
            cpp.size() - optional.size() - 1));
    return std::string(cpp);
}

std::string last_segment(std::string_view qualified) {
    const auto at = qualified.rfind("::");
    return std::string(at == std::string_view::npos ? qualified
                                                    : qualified.substr(at + 2));
}

/*
 * The parse that rejects a malformed token, in the shell unit's own branch
 * order; none when every token converts.
 */
enum class token_parse { enum_type, boolean, value };

std::optional<token_parse> malformed_token_kind(const model_tables& t,
    value_id col) {
    if (values::truthy(t, col, "is_enum"))
        return token_parse::enum_type;
    if (values::str(t, col, "cpp_type") == "bool")
        return token_parse::boolean;
    const auto cpp = unwrapped_cpp_type(values::str(t, col, "cpp_type"));
    if (cpp == "std::string" || cpp == "bool")
        return std::nullopt;
    return token_parse::value;
}

std::string sample_token(const model_tables& t, value_id col) {
    if (values::truthy(t, col, "is_enum")) {
        const auto fallback = values::strip(values::str(t, col,
            "default_value"));
        return fallback.empty() ? "sample" : last_segment(fallback);
    }
    struct sample {
        std::string_view cpp_type;
        std::string_view token;
    };
    static constexpr sample samples[] = {
        {"std::string", "sample"},
        {"int", "1"},
        {"std::int64_t", "1"},
        {"std::uint64_t", "1"},
        {"double", "1.0"},
        {"float", "1.0"},
        {"bool", "false"},
        {"boost::uuids::uuid", "3f2504e0-4f89-11d3-9a0c-0305e82c3301"},
    };
    const auto cpp = unwrapped_cpp_type(values::str(t, col, "cpp_type"));
    const auto i = std::ranges::find(samples, cpp, &sample::cpp_type);
    return std::string(i == std::end(samples) ? "sample" : i->token);
}

void write_malformed_token_column(builder& b, value_id entity,
    const std::string& name, std::int64_t index, token_parse kind) {
    const auto m = b.object(entity, "malformed_token_column");
    b.set(m, "name", name);
    b.set_integer(m, "positional_index", index);
    switch (kind) {
    case token_parse::enum_type:
        b.set(m, "kind", "enum");
        b.set(m, "token", "not-a-member");
        b.set(m, "error", "Invalid " + name + ": ");
        return;
    case token_parse::boolean:
        b.set(m, "kind", "bool");
        b.set(m, "token", "maybe");
        b.set(m, "error", name + " must be 'true' or 'false'.");
        return;
    case token_parse::value:
        b.set(m, "kind", "value");
        b.set(m, "token", "not-a-value");
        b.set(m, "error", "Invalid value for " + name);
        return;
    }
}

}

void derive_supply(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto columns = values::items(t, entity, "columns");
    std::int64_t user_supplied = 0;
    for (const auto c : columns)
        user_supplied += derive_column_supply(b, c, "user") ? 1 : 0;
    const auto pk = primary_key_of(b, entity);
    derive_column_supply(b, pk.key, key_supply(t, pk.key));
    for (const auto m : pk.members)
        user_supplied += derive_column_supply(b, m, key_supply(t, m)) ? 1 : 0;
    b.set_integer(entity, "user_supplied_count", user_supplied);

    std::vector<value_id> typed(columns);
    typed.push_back(pk.key);
    typed.insert(typed.end(), pk.members.begin(), pk.members.end());
    for (const auto c : typed) {
        const std::string cpp_type(values::str(t, c, "cpp_type"));
        b.set(c, "is_bool", cpp_type == "bool");
        if (values::truthy(t, c, "is_enum"))
            b.set(c, "enum_short_name", last_segment(cpp_type));
    }

    b.set(entity, "has_bool_columns", std::ranges::any_of(columns,
        [&](value_id c) {
            return values::truthy(t, c, "is_bool") &&
                values::truthy(t, c, "is_user_supplied");
        }));
    const auto session_party = [&](value_id c) {
        return values::truthy(t, c, "is_session_party");
    };
    b.set(entity, "has_session_party_columns",
        std::ranges::any_of(pk.members, session_party) ||
        std::ranges::any_of(columns, session_party));
}

void derive_sample_tokens(builder& b, value_id entity) {
    const auto& t = b.tables();
    auto read_order = primary_key_of(b, entity).members;
    const auto columns = values::items(t, entity, "columns");
    read_order.insert(read_order.end(), columns.begin(), columns.end());

    std::int64_t index = 0;
    bool malformed_found = false;
    for (const auto c : read_order) {
        if (!values::truthy(t, c, "is_user_supplied"))
            continue;
        b.set_integer(c, "positional_index", index);
        b.set(c, "sample_token", sample_token(t, c));
        if (!malformed_found) {
            if (const auto kind = malformed_token_kind(t, c)) {
                std::string name(values::str(t, c, "name"));
                if (name.empty())
                    name = values::str(t, c, "column");
                write_malformed_token_column(b, entity, name, index, *kind);
                malformed_found = true;
            }
        }
        ++index;
    }
    if (!malformed_found)
        b.set_null(entity, "malformed_token_column");
    b.set_integer(entity, "add_positional_count", index + 2);
}

}

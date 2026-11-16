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
#include <array>
#include <string>
#include <vector>
#include "compass.codegen/ops/values.hpp"
#include "entity_wire.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

std::string join(const std::vector<std::string>& items, std::string_view sep) {
    std::string out;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i != 0)
            out += sep;
        out += items[i];
    }
    return out;
}

/*
 * _and_list: 'a', 'a and b', 'a, b and c'.
 */
std::string and_list(const std::vector<std::string>& names) {
    if (names.size() <= 1)
        return names.empty() ? std::string() : names.front();
    const std::vector<std::string> head(names.begin(), names.end() - 1);
    return join(head, ", ") + " and " + names.back();
}

/*
 * key_is_primary: whether the primary key is exactly the declared key.
 */
bool key_is_primary(const model_tables& t, value_id entity) {
    const auto name = wire::declared_key_field(t, entity);
    if (name.empty())
        return true;
    const auto columns = values::items(t,
        values::member(t, entity, "primary_key"), "columns");
    return columns.size() == 1 && wire::column_name(t, columns[0]) == name;
}

/*
 * The model's own implementation blocks, joined by line ends.
 */
std::string stated_code(const model_tables& t, value_id entity) {
    std::vector<std::string> codes;
    for (const auto block : values::items(t, entity, "implementations")) {
        for (const auto code : values::items(t, block))
            codes.emplace_back(values::str(t, code));
    }
    return join(codes, "\n");
}

/*
 * _prepare_finder_render: the C++ fragments a finder's templates state.
 */
void prepare_finder_render(builder& b, value_id finder) {
    const auto& t = b.tables();
    std::vector<std::string> columns;
    if (values::truthy(t, finder, "columns")) {
        for (const auto c : values::items(t, finder, "columns"))
            columns.push_back(values::py_str(t, c));
    } else if (values::truthy(t, finder, "column")) {
        columns.push_back(values::py_str(t, values::member(t, finder, "column")));
    }
    const auto parent = values::truthy(t, finder, "parent_column") ?
        values::py_str(t, values::member(t, finder, "parent_column")) :
        std::string();
    std::vector<std::string> keys;
    if (!parent.empty())
        keys.push_back(parent);
    keys.insert(keys.end(), columns.begin(), columns.end());

    std::vector<std::string> params, where, service_params, call_args;
    for (const auto& c : keys) {
        params.push_back("const std::string& " + c);
        where.push_back("\"" + c + "\"_c == " + c);
        const auto is_parent = !parent.empty() && c == parent;
        service_params.push_back((is_parent ?
            "const boost::uuids::uuid& " : "const std::string& ") + c);
        call_args.push_back(is_parent ? "boost::uuids::to_string(" + c + ")" : c);
    }
    b.set(finder, "params", join(params, ", "));
    b.set(finder, "where_and", join(where, " && "));
    b.set(finder, "log_label", join(keys, "/"));
    b.set(finder, "log_fields", join(keys, " << \"/\" << "));
    b.set(finder, "columns_label", join(columns, "/"));
    b.set(finder, "key_label", and_list(keys));
    b.set(finder, "key_tail", and_list(columns));
    b.set(finder, "key_noun", keys.size() == 2 ? "pair" :
        keys.size() == 3 ? "triple" : "key");
    b.set(finder, "service_params", join(service_params, ", "));
    b.set(finder, "service_call_args", join(call_args, ", "));
}

constexpr std::array<std::string_view, 13> numeric_cpp_types{
    "int", "long", "long long", "short", "double", "float", "std::int16_t",
    "std::int32_t", "std::int64_t", "std::uint16_t", "std::uint32_t",
    "std::uint64_t", "std::size_t"};

/*
 * _key_as_text: one key column's value as the text a key parameter takes.
 */
std::string key_as_text(const model_tables& t, const std::string& expression,
    value_id column) {
    if (values::truthy(t, column, "is_uuid"))
        return "boost::uuids::to_string(" + expression + ")";
    if (values::truthy(t, column, "is_timestamp"))
        return "ores::platform::time::datetime::to_db_string(" + expression + ")";
    if (std::ranges::find(numeric_cpp_types,
            values::str(t, column, "cpp_type")) != numeric_cpp_types.end())
        return "std::to_string(" + expression + ")";
    return expression;
}

bool is_word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_';
}

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
        c == '\v';
}

/*
 * A cursor over text that matches the tokens of states_batch_finder's
 * pattern, \s* between them.
 */
struct cursor {
    std::string_view s;
    std::size_t at;

    void spaces() {
        while (at < s.size() && is_space(s[at]))
            ++at;
    }
    bool literal(std::string_view l) {
        if (s.substr(at, l.size()) != l)
            return false;
        at += l.size();
        return true;
    }
    bool token(std::string_view l) {
        spaces();
        return literal(l);
    }
    bool spaced_word(std::string_view l) {
        spaces();
        if (!literal(l))
            return false;
        if (at >= s.size() || !is_space(s[at]))
            return false;
        return true;
    }
    void words() {
        while (at < s.size() && is_word(s[at]))
            ++at;
    }
};

/*
 * states_batch_finder: read_latest_by_<key>\s*\(\s*const\s+context\s*&
 * \s*\w*\s*,\s*const\s+std::vector\s*<\s*std::string\s*>\s*& in @p code.
 */
bool states_batch_finder(std::string_view code, const std::string& key) {
    if (key.empty())
        return false;
    const auto name = "read_latest_by_" + key;
    for (auto at = code.find(name); at != std::string_view::npos;
         at = code.find(name, at + 1)) {
        cursor c{code, at + name.size()};
        if (!c.token("(") || !c.spaced_word("const") || !c.token("context") ||
            !c.token("&"))
            continue;
        c.spaces();
        c.words();
        if (!c.token(",") || !c.spaced_word("const") ||
            !c.token("std::vector") || !c.token("<") ||
            !c.token("std::string") || !c.token(">") || !c.token("&"))
            continue;
        return true;
    }
    return false;
}

}

void derive_key_finders(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto legacy = values::member(t, entity, "service_find_by_code");
    std::vector<std::string> legacy_columns;
    if (values::truthy(t, legacy, "columns")) {
        for (const auto c : values::items(t, legacy, "columns"))
            legacy_columns.push_back(values::py_str(t, c));
    } else if (values::truthy(t, legacy, "column")) {
        legacy_columns.push_back(values::py_str(t,
            values::member(t, legacy, "column")));
    }
    const auto parent = values::truthy(t, legacy, "parent_column") ?
        values::py_str(t, values::member(t, legacy, "parent_column")) :
        std::string();
    const auto declared = wire::declared_key_field(t, entity);
    const auto primary = key_is_primary(t, entity);
    const auto stated = stated_code(t, entity);

    const auto finders = b.list(entity, "key_finders");
    bool declared_stated = false;
    if (!legacy_columns.empty()) {
        const auto f = b.append_object(finders);
        b.set(f, "column", legacy_columns.front());
        b.set(f, "suffix", "code");
        if (legacy_columns.size() > 1) {
            const auto list = b.list(f, "columns");
            for (const auto& c : legacy_columns)
                b.append(list, c);
        }
        if (!parent.empty())
            b.set(f, "parent_column", parent);
        declared_stated = legacy_columns.front() == declared &&
            declared == "code";
    }
    if (!declared.empty() && !primary && !declared_stated &&
        stated.find("read_latest_by_" + declared) == std::string::npos) {
        const auto f = b.append_object(finders);
        b.set(f, "column", declared);
        b.set(f, "suffix", declared);
    }
    for (const auto f : values::items(t, finders))
        prepare_finder_render(b, f);
    if (values::truthy(t, legacy))
        prepare_finder_render(b, legacy);

    const auto resolvers = b.list(entity, "key_resolvers");
    if (!declared.empty() && !primary) {
        const auto r = b.append_object(resolvers);
        b.set(r, "column", declared);
        b.set(r, "suffix", declared);
    }
}

void derive_declared_key(builder& b, value_id entity) {
    const auto& t = b.tables();
    const auto declared = wire::declared_key_field(t, entity);
    const auto primary = key_is_primary(t, entity);
    b.set(entity, "declared_key", declared);
    b.set(entity, "key_is_primary", primary);

    bool scoped = false;
    for (const auto c : values::items(t, entity, "natural_keys")) {
        const auto name = wire::column_name(t, c);
        scoped = scoped || (name != declared && name != "tenant_id");
    }
    b.set(entity, "history_by_row_id", !declared.empty() && !primary &&
        scoped && values::truthy(t, entity, "service_find_by_uuid"));

    const auto column = wire::declared_key_column(t, entity);
    const auto found = !column.is_none();
    const auto key_text = "key." + declared;
    b.set(entity, "declared_key_text",
        found ? key_as_text(t, key_text, column) : key_text);
    const std::string prefix(values::str(t, column, "group_prefix"));
    const auto row_text = "row." + prefix + declared;
    b.set(entity, "declared_row_text",
        found ? key_as_text(t, row_text, column) : row_text);

    bool batch = false;
    for (const auto f : values::items(t, entity, "key_finders")) {
        const auto is_declared = !primary &&
            values::py_str(t, values::member(t, f, "column")) == declared &&
            values::py_str(t, values::member(t, f, "suffix")) == declared &&
            !values::truthy(t, f, "columns") &&
            !values::truthy(t, f, "parent_column");
        b.set(f, "is_declared_key_finder", is_declared);
        batch = batch || is_declared;
    }
    b.set(entity, "declared_key_has_batch_read",
        batch || states_batch_finder(stated_code(t, entity), declared));
    b.set(entity, "declared_key_group_prefix", prefix);
    b.set(entity, "declared_key_is_uuid", found &&
        (values::py_str(t, values::member(t, column, "cpp_type")).find(
             "boost::uuids::uuid") != std::string::npos ||
         values::str(t, column, "type") == "uuid"));
}

}

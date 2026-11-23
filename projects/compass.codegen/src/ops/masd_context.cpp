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
#include "compass.codegen/ops/masd_context.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace compass::codegen::ops {

namespace {

namespace mtypes = compass::model::types;
using compass::model::ops::builder;
using mtypes::key_kind;
using mtypes::masd_column;
using mtypes::masd_entity;
using mtypes::value_id;
using mtypes::written_by;

/**
 * @brief The C++ type of each SQL type a row may hold, when the model
 * states none.
 */
constexpr std::pair<std::string_view, std::string_view> sql_to_cpp[] = {
    {"text", "std::string"},
    {"integer", "std::int32_t"},
    {"bigint", "std::int64_t"},
    {"boolean", "bool"},
    {"bytea", "std::string"},
    {"jsonb", "std::string"},
    {"text[]", "std::vector<std::string>"},
    {"timestamptz", "std::chrono::sys_time<std::chrono::microseconds>"}};

/**
 * @brief The standard header each standard name needs, the first of a
 * name's spellings that a type mentions winning.
 */
constexpr std::pair<std::string_view, std::string_view> std_headers[] = {
    {"std::string_view", "<string_view>"},
    {"std::string", "<string>"},
    {"std::optional", "<optional>"},
    {"std::vector", "<vector>"},
    {"std::chrono", "<chrono>"},
    {"std::int", "<cstdint>"},
    {"std::uint", "<cstdint>"},
    {"std::size_t", "<cstddef>"}};

bool is_word(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

/**
 * @brief Whether @p type mentions @p name as a whole name, so
 * std::string does not match std::string_view.
 */
bool mentions(std::string_view type, std::string_view name) {
    for (auto at = type.find(name); at != std::string_view::npos;
         at = type.find(name, at + 1)) {
        const auto end = at + name.size();
        const bool prefix = name.ends_with("int");
        if (prefix || end == type.size() || !is_word(type[end]))
            return true;
    }
    return false;
}

std::string upper(std::string_view s) {
    std::string out;
    for (const char c : s)
        out += is_word(c) ? static_cast<char>(std::toupper(
            static_cast<unsigned char>(c))) : '_';
    return out;
}

bool in_row(const masd_column& c) {
    return c.writer == written_by::row && (c.key == key_kind::none ||
        c.key == key_kind::natural);
}

bool in_copy(const masd_column& c) {
    return c.writer == written_by::row && c.key != key_kind::identity;
}

bool is_parameter(const masd_column& c) {
    return c.writer == written_by::row && !in_row(c) &&
        c.key != key_kind::identity;
}

std::string base_type(const masd_column& c) {
    if (!c.cpp_type.empty())
        return c.cpp_type;
    if (!c.enum_name.empty())
        return c.enum_name;
    for (const auto& [sql, cpp] : sql_to_cpp)
        if (sql == c.sql_type)
            return std::string(cpp);
    throw std::runtime_error("codegen: the column '" + c.name + "' is of "
        "SQL type " + c.sql_type + ", which has no C++ type; state one with "
        ":cpp_type:");
}

std::string member_type(const masd_column& c) {
    const auto t = base_type(c);
    return c.nullable ? "std::optional<" + t + ">" : t;
}

bool is_arithmetic(std::string_view t) {
    return t == "bool" || t == "std::size_t" || t.starts_with("std::int") ||
        t.starts_with("std::uint");
}

/**
 * @brief The value a member starts with: zero, false, or the value of its
 * enumeration the column defaults to, else the first.
 */
std::string initializer(const masd_column& c) {
    if (c.nullable)
        return {};
    const auto t = base_type(c);
    if (!c.enum_name.empty() && c.cpp_type.empty()) {
        auto value = c.values.front();
        const auto& d = c.default_value;
        if (d.size() > 2 && d.front() == '\'' && d.back() == '\'')
            value = d.substr(1, d.size() - 2);
        return " = " + c.enum_name + "::" + value;
    }
    if (t == "bool")
        return " = false";
    if (is_arithmetic(t))
        return " = 0";
    return {};
}

/**
 * @brief The includes a header holding @p types needs, standard headers
 * first, then @p project and the header of each custom type, each sorted.
 */
void add_includes(builder& b, value_id parent,
    const std::vector<std::string>& types,
    const compass::model::ops::custom_types& custom,
    std::set<std::string> project) {
    std::set<std::string> system;
    for (const auto& t : types) {
        for (const auto& [name, header] : std_headers)
            if (mentions(t, name))
                system.emplace(header);
        for (const auto& [name, header] : custom)
            if (mentions(t, name))
                project.insert(header);
    }
    const auto list = b.list(parent, "includes");
    for (const auto* set : {&system, &project})
        for (const auto& h : *set)
            b.set(b.append_object(list), "include", h);
}

void add_last(builder& b, value_id item, bool last) {
    b.set(item, "last", last);
}

/**
 * @brief @p words joined by ", " into lines of string literals no longer
 * than @p width, each but the last ending in the separator.
 */
std::vector<std::string> wrapped(const std::vector<std::string>& words,
    std::size_t width) {
    std::vector<std::string> lines(1);
    for (std::size_t i = 0; i < words.size(); ++i) {
        const auto piece = words[i] + (i + 1 < words.size() ? ", " : "");
        if (!lines.back().empty() && lines.back().size() + piece.size() >
            width)
            lines.emplace_back();
        lines.back() += piece;
    }
    return lines;
}

value_id with_library(builder& b, value_id library) {
    const auto context = b.detached_object();
    const auto& t = b.tables();
    for (auto c = t.values.firsts[library]; !c.is_none();
         c = t.values.nexts[c])
        b.set_copy(context, mtypes::text_of(t.texts, t.values.keys[c]), c);
    return context;
}

void add_enums(builder& b, value_id m, const masd_entity& e) {
    const auto list = b.list(m, "enums");
    for (const auto& c : e.columns) {
        if (c.values.empty() || !c.cpp_type.empty())
            continue;
        const auto en = b.append_object(list);
        b.set(en, "name", c.enum_name);
        b.set(en, "column", c.name);
        b.set(en, "first", c.values.front());
        const auto values = b.list(en, "values");
        for (std::size_t i = 0; i < c.values.size(); ++i) {
            const auto v = b.append_object(values);
            b.set(v, "value", c.values[i]);
            add_last(b, v, i + 1 == c.values.size());
        }
    }
}

std::string parameter_of(const masd_column& c) {
    const auto t = c.key == key_kind::ord ? std::string("std::size_t")
        : base_type(c);
    return (is_arithmetic(t) ? t + " " : "const " + t + "& ") + c.member;
}

void add_copy(builder& b, value_id m, const masd_entity& e) {
    std::vector<std::string> columns;
    const auto parameters = b.list(m, "copy_parameters");
    const auto fields = b.list(m, "copy_fields");
    for (const auto& c : e.columns) {
        if (!in_copy(c))
            continue;
        columns.push_back(c.name);
        if (is_parameter(c))
            b.set(b.append_object(parameters), "declaration", parameter_of(c));
        b.set(b.append_object(fields), "expression",
            is_parameter(c) ? c.member : "r." + c.member);
    }
    const auto lines = wrapped(columns, 68);
    const auto list = b.list(m, "copy_column_lines");
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto l = b.append_object(list);
        b.set(l, "text", lines[i]);
        add_last(b, l, i + 1 == lines.size());
    }
}

std::string column_definition(const masd_column& c,
    const masd_entity& e, bool single_key) {
    auto out = c.name + " " + c.sql_type;
    if (c.key == key_kind::identity)
        out += " GENERATED ALWAYS AS IDENTITY";
    if (!c.generated.empty())
        out += " GENERATED ALWAYS AS (" + c.generated + ") STORED";
    if (c.primary_key && single_key)
        out += " PRIMARY KEY";
    else if (!c.nullable)
        out += " NOT NULL";
    if (c.unique)
        out += " UNIQUE";
    if (!c.default_value.empty())
        out += " DEFAULT " + c.default_value;
    for (const auto& f : e.foreign_keys) {
        if (f.column != c.name)
            continue;
        out += " REFERENCES " + f.references;
        if (f.cascade)
            out += " ON DELETE CASCADE";
    }
    if (!c.values.empty()) {
        out += " CHECK (" + c.name + " IN (";
        for (std::size_t i = 0; i < c.values.size(); ++i)
            out += (i ? ", '" : "'") + c.values[i] + "'";
        out += "))";
    }
    return out;
}

std::string joined(const std::vector<std::string>& names) {
    std::string out;
    for (const auto& n : names)
        out += (out.empty() ? "" : ", ") + n;
    return out;
}

std::string qualified(const masd_entity& e) {
    return e.schema + "." + e.entity;
}

void add_table(builder& b, value_id list, const masd_entity& e) {
    const auto t = b.append_object(list);
    b.set(t, "table", qualified(e));
    b.set(t, "retained", e.retained);
    std::vector<std::string> definitions;
    std::vector<std::string> keys;
    for (const auto& c : e.columns)
        if (c.primary_key)
            keys.push_back(c.name);
    for (const auto& c : e.columns)
        definitions.push_back(column_definition(c, e, keys.size() == 1));
    if (keys.size() > 1)
        definitions.push_back("PRIMARY KEY (" + joined(keys) + ")");
    for (const auto& u : e.uniques)
        definitions.push_back("UNIQUE (" + joined(u) + ")");
    for (const auto& check : e.checks)
        definitions.push_back("CHECK (" + check + ")");
    const auto defs = b.list(t, "definitions");
    for (std::size_t i = 0; i < definitions.size(); ++i) {
        const auto d = b.append_object(defs);
        b.set(d, "definition", definitions[i]);
        add_last(b, d, i + 1 == definitions.size());
    }
    const auto indexes = b.list(t, "indexes");
    for (const auto& i : e.indexes) {
        std::vector<std::string> columns;
        for (const auto& c : i.columns)
            columns.push_back(i.opclass.empty() ? c : c + " " + i.opclass);
        auto s = std::string(i.unique ? "CREATE UNIQUE INDEX " :
            "CREATE INDEX ") + e.entity + "_" + i.name + " ON " +
            qualified(e);
        if (!i.method.empty())
            s += " USING " + i.method;
        s += " (" + joined(columns) + ")";
        if (!i.where.empty())
            s += " WHERE " + i.where;
        b.set(b.append_object(indexes), "statement", s + ";");
    }
}

/**
 * @brief The tables of @p schema among @p tables, each after the tables it
 * references, ties broken by name.
 */
std::vector<const masd_entity*> ordered(std::string_view schema,
    std::span<const masd_entity> tables, std::set<std::string>& created) {
    std::map<std::string, const masd_entity*> pending;
    for (const auto& e : tables)
        if (e.schema == schema)
            pending.emplace(qualified(e), &e);
    std::vector<const masd_entity*> out;
    while (!pending.empty()) {
        const auto ready = std::ranges::find_if(pending, [&](const auto& p) {
            return std::ranges::all_of(p.second->foreign_keys,
                [&](const auto& f) {
                    return f.references == p.first ||
                        created.contains(f.references);
                });
        });
        if (ready == pending.end())
            throw std::runtime_error("codegen: the tables of the schema " +
                std::string(schema) + " reference a table no listed schema "
                "creates before them, or each other in a cycle: " +
                pending.begin()->first);
        created.insert(ready->first);
        out.push_back(ready->second);
        pending.erase(ready);
    }
    return out;
}

}

value_id masd_entity_context(builder& b, value_id library,
    const masd_entity& e, const compass::model::ops::custom_types& custom) {
    const auto context = with_library(b, library);
    const auto m = b.object(context, "masd_entity");
    const auto component = e.product + "." + e.component;
    const auto ns = e.product + "::" + e.component;
    b.set(m, "table", qualified(e));
    b.set(m, "entity", e.entity);
    b.set(m, "row_type", e.row_type);
    b.set(m, "types_namespace", ns + "::types");
    b.set(m, "io_namespace", ns + "::io");
    b.set(m, "row_guard", upper(component + "/types/" + e.entity +
        "_row.hpp"));
    b.set(m, "copy_guard", upper(component + "/io/" + e.entity +
        "_copy.hpp"));
    b.set(m, "row_include", component + "/types/" + e.entity + "_row.hpp");
    b.set(m, "copy_include", component + "/io/" + e.entity + "_copy.hpp");
    b.set(m, "brief", e.brief);
    std::vector<std::string> row_types;
    for (const auto& c : e.columns) {
        if (in_row(c))
            row_types.push_back(member_type(c));
        if (!c.values.empty())
            row_types.insert(row_types.end(),
                {"std::optional", "std::string_view", "std::uint8_t"});
    }
    add_includes(b, m, row_types, custom, {});
    add_enums(b, m, e);
    const auto members = b.list(m, "members");
    for (const auto& c : e.columns) {
        if (!in_row(c))
            continue;
        const auto member = b.append_object(members);
        b.set(member, "member", c.member);
        b.set(member, "cpp_type", member_type(c));
        b.set(member, "initializer", initializer(c));
    }
    std::vector<std::string> parameter_types{"std::string_view"};
    for (const auto& c : e.columns)
        if (is_parameter(c))
            parameter_types.push_back(c.key == key_kind::ord
                ? "std::size_t" : base_type(c));
    const auto copy = b.object(m, "copy");
    add_includes(b, copy, parameter_types, custom,
        {"\"" + component + "/ops/copy_text.hpp\"",
            "\"" + component + "/types/" + e.entity + "_row.hpp\""});
    add_copy(b, m, e);
    return context;
}

value_id masd_schema_context(builder& b, value_id library,
    const mtypes::masd_schema& s, std::span<const masd_entity> tables) {
    const auto context = with_library(b, library);
    const auto m = b.object(context, "masd_schema");
    const auto schemas = b.list(m, "schemas");
    std::set<std::string> created;
    for (const auto& name : s.schemas) {
        const auto schema = b.append_object(schemas);
        b.set(schema, "name", name);
        const auto list = b.list(schema, "tables");
        for (const auto* e : ordered(name, tables, created))
            add_table(b, list, *e);
    }
    const auto functions = b.list(m, "functions");
    for (const auto& f : s.functions) {
        auto sql = f;
        while (!sql.empty() && std::isspace(
                   static_cast<unsigned char>(sql.back())))
            sql.pop_back();
        b.set(b.append_object(functions), "sql", sql);
    }
    return context;
}

}

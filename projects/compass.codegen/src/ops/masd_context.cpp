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

void add_enums(builder& b, value_id m, const masd_entity& e,
    std::string_view types_namespace) {
    const auto list = b.list(m, "enums");
    bool any = false;
    for (const auto& c : e.columns) {
        if (c.values.empty() || !c.cpp_type.empty())
            continue;
        any = true;
        const auto en = b.append_object(list);
        b.set(en, "name", c.enum_name);
        b.set(en, "qualified", std::string(types_namespace) + "::" +
            c.enum_name);
        b.set(en, "column", c.name);
        b.set(en, "first", c.values.front());
        std::string listed;
        for (const auto& v : c.values)
            listed += (listed.empty() ? "" : ", ") + v;
        b.set(en, "listed", listed);
        const auto values = b.list(en, "values");
        for (std::size_t i = 0; i < c.values.size(); ++i) {
            const auto v = b.append_object(values);
            b.set(v, "value", c.values[i]);
            add_last(b, v, i + 1 == c.values.size());
        }
    }
    b.set(m, "has_enums", any);
}

/**
 * @brief The members of the row's wire shape, in row order, and the
 * headers that state the text form of each custom type they hold, sorted.
 */
void add_codec(builder& b, value_id m, const masd_entity& e,
    const compass::model::ops::custom_types& text_forms) {
    std::set<std::string> headers;
    const auto fields = b.list(m, "wire_fields");
    for (const auto& c : e.columns) {
        if (!in_row(c))
            continue;
        b.set(b.append_object(fields), "member", c.member);
        for (const auto& [name, header] : text_forms)
            if (mentions(member_type(c), name))
                headers.insert(header);
    }
    const auto list = b.list(m, "codec_includes");
    for (const auto& h : headers)
        b.set(b.append_object(list), "include", h);
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
    const masd_entity& e, const compass::model::ops::custom_types& custom,
    const compass::model::ops::custom_types& text_forms) {
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
    b.set(m, "row_qualified", ns + "::types::" + e.row_type);
    b.set(m, "codec_namespace", ns + "::codec");
    b.set(m, "codec_root", component + "/codec");
    b.set(m, "codec_guard", upper(component + "/types/" + e.entity +
        "_row_codec.hpp"));
    b.set(m, "codec_include", component + "/types/" + e.entity +
        "_row_codec.hpp");
    b.set(m, "copy_include", component + "/io/" + e.entity + "_copy.hpp");
    b.set(m, "brief", e.brief);
    std::vector<std::string> row_types;
    for (const auto& c : e.columns) {
        if (in_row(c))
            row_types.push_back(member_type(c));
        if (!c.values.empty())
            row_types.insert(row_types.end(),
                {"std::optional", "std::string_view"});
    }
    add_includes(b, m, row_types, custom, {});
    add_enums(b, m, e, ns + "::types");
    add_codec(b, m, e, text_forms);
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

namespace {

/**
 * @brief The type @p cpp_type holds: itself, or what an optional or a
 * vector of it holds.
 */
std::string held_type(std::string_view cpp_type) {
    for (const std::string_view wrapper : {"std::optional<", "std::vector<"})
        if (cpp_type.starts_with(wrapper) && cpp_type.ends_with(">"))
            return held_type(cpp_type.substr(wrapper.size(),
                cpp_type.size() - wrapper.size() - 1));
    return std::string(cpp_type);
}

/**
 * @brief @p messages, each after the messages of the same model its
 * fields hold, ties kept in model order.
 * @throws std::runtime_error when the messages hold each other in a cycle.
 */
std::vector<const mtypes::masd_message*> declaration_order(
    const std::vector<mtypes::masd_message>& messages) {
    std::set<std::string> names;
    for (const auto& m : messages)
        names.insert(m.name);
    std::set<std::string> declared;
    std::vector<const mtypes::masd_message*> out;
    std::vector<const mtypes::masd_message*> pending;
    for (const auto& m : messages)
        pending.push_back(&m);
    while (!pending.empty()) {
        const auto ready = std::ranges::find_if(pending, [&](const auto* m) {
            return std::ranges::all_of(m->fields, [&](const auto& f) {
                const auto t = held_type(f.cpp_type);
                return t == m->name || !names.contains(t) ||
                    declared.contains(t);
            });
        });
        if (ready == pending.end())
            throw std::runtime_error("codegen: the messages " +
                (*pending.begin())->name + " and others hold each other in "
                "a cycle");
        declared.insert((*ready)->name);
        out.push_back(*ready);
        pending.erase(ready);
    }
    return out;
}

bool is_standard_or_arithmetic(std::string_view t) {
    return t.starts_with("std::") || t == "bool" || t == "double" ||
        t == "float";
}

/**
 * @brief @p text as the body of a C++ string literal on one line: its lines
 * joined by a space, quotes and backslashes escaped.
 */
std::string literal(std::string_view text) {
    std::string out;
    bool space = false;
    for (const char c : text) {
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            space = !out.empty();
            continue;
        }
        if (space)
            out += ' ';
        space = false;
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    return out;
}

/**
 * @brief @p names as the items of a C++ initializer list of string
 * literals.
 */
std::string quoted_list(const std::vector<std::string>& names) {
    std::string out;
    for (const auto& n : names)
        out += (out.empty() ? "\"" : ", \"") + n + "\"";
    return out;
}

/**
 * @brief Whether a call may leave @p f out, by the service overview's
 * binding rules: an optional, a vector or a message, a field with a
 * default, or one stating :required: false.
 */
bool may_leave_out(const mtypes::masd_field& f,
    const std::set<std::string>& messages) {
    const std::string_view t = f.cpp_type;
    return f.optional || !f.default_value.empty() ||
        t.starts_with("std::optional<") || t.starts_with("std::vector<") ||
        messages.contains(f.cpp_type);
}

/**
 * @brief The OpenAPI type of a query parameter of C++ type @p t.
 */
std::string_view openapi_type(std::string_view t) {
    if (t.starts_with("std::optional<"))
        t = t.substr(14, t.size() - 15);
    if (t == "bool")
        return "boolean";
    if (is_arithmetic(t))
        return "integer";
    return "string";
}

/**
 * @brief The routes of @p op's request messages under @p m: for each, its
 * declaration in the copied builder's vocabulary and how its request is
 * bound, a message-typed query field spread into one parameter per member.
 */
void add_routes(builder& b, value_id m, const mtypes::masd_operation& op,
    const std::set<std::string>& names) {
    const auto message_named = [&](std::string_view name)
        -> const mtypes::masd_message* {
        for (const auto& msg : op.messages)
            if (msg.name == name)
                return &msg;
        return nullptr;
    };
    const auto list = b.list(m, "routes");
    bool any = false;
    for (const auto& msg : op.messages) {
        const auto& r = msg.route;
        if (r.method.empty())
            continue;
        any = true;
        const auto item = b.append_object(list);
        b.set(item, "tool", r.tool);
        b.set(item, "area", r.area);
        b.set(item, "entity", r.entity);
        b.set(item, "writes", r.writes ? "true" : "false");
        b.set(item, "summary", literal(r.summary));
        b.set(item, "description", literal(msg.description));
        b.set(item, "path", r.path);
        b.set(item, "method", upper(r.method));
        b.set(item, "builder", r.method == "delete" ? "delete_" : r.method);
        b.set(item, "caller", r.caller);
        b.set(item, "request", op.ns + "::" + msg.name);
        b.set(item, "has_response", !msg.response.empty());
        b.set(item, "response", msg.response.empty() ? std::string("void")
            : op.ns + "::" + msg.response);
        b.set(item, "max_body_size", r.max_body_size);
        b.set(item, "has_max_body_size", !r.max_body_size.empty());

        const auto params = b.list(item, "query_params");
        const auto paths = b.list(item, "path_fields");
        const auto queries = b.list(item, "query_fields");
        const auto checks = b.list(item, "checks");
        std::vector<std::string> body, required_body;
        const auto add_query = [&](std::string member, std::string param,
                                   const mtypes::masd_field& f, bool required) {
            const auto q = b.append_object(queries);
            b.set(q, "member", member);
            b.set(q, "param", param);
            b.set(q, "required", required ? "true" : "false");
            const auto p = b.append_object(params);
            b.set(p, "name", param);
            b.set(p, "type", std::string(openapi_type(f.cpp_type)));
            b.set(p, "required", required ? "true" : "false");
            b.set(p, "description", literal(f.description));
            if (!f.values.empty() && held_type(f.cpp_type) == "std::string") {
                const auto c = b.append_object(checks);
                b.set(c, "member", member);
                b.set(c, "param", param);
                b.set(c, "values", quoted_list(f.values));
            }
        };
        for (const auto& f : msg.fields) {
            if (f.http_in == "path") {
                const auto p = b.append_object(paths);
                b.set(p, "member", f.name);
                b.set(p, "param", f.name);
            } else if (f.http_in == "body") {
                body.push_back(f.name);
                if (!may_leave_out(f, names))
                    required_body.push_back(f.name);
            } else if (const auto* inner = message_named(f.cpp_type)) {
                for (const auto& g : inner->fields)
                    add_query(f.name + "." + g.name,
                        g.http_name.empty() ? g.name : g.http_name, g,
                        !may_leave_out(g, names));
            } else {
                add_query(f.name, f.http_name.empty() ? f.name : f.http_name,
                    f, !may_leave_out(f, names));
            }
        }
        b.set(item, "has_body", !body.empty());
        b.set(item, "body_members", quoted_list(body));
        b.set(item, "body_required", quoted_list(required_body));
        b.set(item, "takes_no_body", r.method != "get" && body.empty());
    }
    b.set(m, "has_routes", any);
}

}

value_id masd_operation_context(builder& b, value_id library,
    const mtypes::masd_operation& op, std::string_view product,
    std::string_view codec_component) {
    const auto context = with_library(b, library);
    const auto m = b.object(context, "masd_operation");
    const auto component = std::string(product) + "." + op.component;
    const auto codec = std::string(product) + "." + std::string(codec_component);
    const auto codec_ns = std::string(product) + "::" +
        std::string(codec_component) + "::codec";
    b.set(m, "codec_namespace", codec_ns);
    b.set(m, "codec_root", codec + "/codec");
    b.set(m, "namespace", op.ns);
    b.set(m, "entity", op.entity);
    b.set(m, "protocol_guard", upper(component + "/messages/" + op.entity +
        "_protocol.hpp"));
    b.set(m, "protocol_include", component + "/messages/" + op.entity +
        "_protocol.hpp");
    b.set(m, "component_root", component);
    b.set(m, "component_namespace", std::string(product) + "::" +
        op.component);
    b.set(m, "routes_guard", upper(component + "/routes/" + op.entity +
        "_routes.hpp"));
    b.set(m, "routes_include", component + "/routes/" + op.entity +
        "_routes.hpp");

    std::set<std::string> message_names;
    for (const auto& msg : op.messages)
        message_names.insert(msg.name);
    add_routes(b, m, op, message_names);
    bool base64 = false;
    std::map<std::string, std::vector<std::string>> enums;
    for (const auto& msg : op.messages)
        for (const auto& f : msg.fields) {
            base64 = base64 || f.encoding == "base64";
            const auto t = held_type(f.cpp_type);
            if (f.values.empty() || is_standard_or_arithmetic(t) ||
                message_names.contains(t))
                continue;
            const auto [at, added] = enums.try_emplace(t, f.values);
            if (!added && at->second != f.values)
                throw std::runtime_error("codegen: the enumeration " + t +
                    " lists different values in " + msg.name + "." + f.name);
        }
    b.set(m, "has_base64", base64);
    std::set<std::string> standard{"<optional>", "<stdexcept>", "<string>",
        "<string_view>", "<type_traits>"};
    std::set<std::string> project;
    for (const auto& i : op.includes)
        (i.starts_with("<") ? standard : project).insert(i);
    const auto includes = b.list(m, "includes");
    for (const auto* set : {&standard, &project})
        for (const auto& i : *set)
            b.set(b.append_object(includes), "include", i);

    const auto enum_list = b.list(m, "enums");
    for (const auto& [name, values] : enums) {
        const auto en = b.append_object(enum_list);
        b.set(en, "name", name);
        b.set(en, "qualified", op.ns + "::" + name);
        auto dotted = op.ns;
        for (auto at = dotted.find("::"); at != std::string::npos;
             at = dotted.find("::", at + 1))
            dotted.replace(at, 2, ".");
        b.set(en, "guard", upper(dotted + "." + name + ".enum"));
        b.set(en, "first", values.front());
        std::string listed;
        for (const auto& v : values)
            listed += (listed.empty() ? "" : ", ") + v;
        b.set(en, "listed", listed);
        const auto list = b.list(en, "values");
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto v = b.append_object(list);
            b.set(v, "value", values[i]);
            add_last(b, v, i + 1 == values.size());
        }
    }

    const auto list = b.list(m, "messages");
    for (const auto* msg : declaration_order(op.messages)) {
        const auto item = b.append_object(list);
        b.set(item, "name", msg->name);
        b.set(item, "qualified", op.ns + "::" + msg->name);
        const auto fields = b.list(item, "fields");
        for (const auto& f : msg->fields) {
            const auto field = b.append_object(fields);
            b.set(field, "member", f.name);
            b.set(field, "message", msg->name);
            const auto type = f.encoding == "base64"
                ? codec_ns + "::base64_bytes" : f.cpp_type;
            b.set(field, "cpp_type", type);
            const auto held = held_type(f.cpp_type);
            std::string init;
            if (!f.default_value.empty())
                init = " = " + (f.encoding == "base64"
                    ? codec_ns + "::base64_bytes{" + f.default_value + "}"
                    : f.default_value);
            else if (type == f.cpp_type && held == f.cpp_type &&
                (enums.contains(held) || is_arithmetic(held) ||
                    held == "double"))
                init = "{}";
            b.set(field, "initializer", init);
        }
    }
    return context;
}

value_id masd_component_context(builder& b, value_id library,
    std::string_view component_dir, std::string_view codec_dir) {
    const auto context = with_library(b, library);
    const auto m = b.object(context, "masd_component");
    const auto namespace_of = [](std::string_view dir) {
        std::string ns;
        for (const char c : dir)
            ns += c == '.' ? std::string("::") : std::string(1, c);
        return ns;
    };
    const auto ns = namespace_of(component_dir);
    b.set(m, "codec_namespace", ns + "::codec");
    b.set(m, "codec_root", std::string(component_dir) + "/codec");
    b.set(m, "codec_guard_prefix", upper(std::string(component_dir) +
        "/codec/"));
    b.set(m, "component_namespace", ns);
    b.set(m, "component_root", std::string(component_dir));
    b.set(m, "guard_prefix", upper(std::string(component_dir) + "/"));
    const auto wire = codec_dir.empty() ? component_dir : codec_dir;
    b.set(m, "wire_namespace", namespace_of(wire) + "::codec");
    b.set(m, "wire_root", std::string(wire) + "/codec");
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

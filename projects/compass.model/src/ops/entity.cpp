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
#include <algorithm>
#include <set>
#include <string>
#include "entity.hpp"
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::value_id;
using types::value_kind;

constexpr std::string_view protocol_messages_paste_kind =
    "2C4E8F1A-6B9D-4A3E-8F2C-7D1E5A9B3C6F";

constexpr std::string_view entity_header_standard_includes[] = {
    "<string>", "<optional>", "<ostream>",
    "\"ores.database/repository/db_types.hpp\"", "<boost/uuid/uuid.hpp>",
    "<cstdint>"};

constexpr std::string_view sql_string_types[] = {
    "text", "string", "varchar", "char"};

/**
 * @brief The namespace of each feature a profile may assign: "" for the
 * entity itself, else the sub-object it belongs to.
 */
constexpr std::pair<std::string_view, std::string_view> feature_namespaces[] = {
    {"has_tenant_id", ""}, {"has_parent_id", ""}, {"read_only", ""},
    {"sql_only", ""}, {"system_scope", "sql"}, {"nullable_tenant_id", "sql"},
    {"no_audit_columns", "sql"}, {"current_state", "sql"},
    {"append_insert", "sql"}, {"extra_checks", "sql"},
    {"extra_delete_sets", "sql"}, {"fk_copy_validations", "sql"},
    {"text_code_validations", "sql"}, {"party_id_from_book_id", "sql"},
    {"party_id_from_parent", "sql"}, {"party_id_from_session", "sql"},
    {"rls_tenant_isolation", "sql"}, {"rls_party_isolation", "sql"},
    {"rls_account_party_isolation", "sql"},
    {"rls_system_tenant_visible", "sql"},
    {"rls_own_or_system_tenant_rows", "sql"},
    {"rls_service_read_role", "sql"}, {"rls_policy_style", "sql"},
    {"writer_role", "sql"}, {"has_pagination", "presentation"},
    {"has_uuid_primary_key", "presentation"},
    {"has_change_reason_cache", "presentation"},
    {"has_explorer_api", "presentation"},
    {"parent_entity_singular", "presentation"},
    {"has_csv_xml_io", "presentation"}, {"has_export_macro", "presentation"},
    {"has_version_navigation", "presentation"},
    {"has_readonly_paginated_list", "presentation"},
    {"has_parent_scoped_list", "presentation"},
    {"parent_key_field", "presentation"},
    {"parent_key_param", "presentation"},
};

std::optional<std::string_view> namespace_of(std::string_view feature) {
    for (const auto& [f, ns] : feature_namespaces) {
        if (f == feature)
            return ns;
    }
    return std::nullopt;
}

template <std::size_t N>
bool one_of(std::string_view s, const std::string_view (&set)[N]) {
    return std::ranges::find(set, s) != std::end(set);
}

std::string strip_chars(std::string_view s, std::string_view chars) {
    while (!s.empty() && chars.find(s.front()) != std::string_view::npos)
        s.remove_prefix(1);
    while (!s.empty() && chars.find(s.back()) != std::string_view::npos)
        s.remove_suffix(1);
    return std::string(s);
}

std::string sql_literal_text(std::string_view s) {
    std::string out;
    for (const char ch : s) {
        out += ch;
        if (ch == '\'')
            out += '\'';
    }
    return out;
}

std::string sql_quote_default(std::string_view raw) {
    const auto stripped = trim(raw);
    if (stripped.empty() || stripped.find('(') != std::string_view::npos ||
        stripped.front() == '\'' || stripped.front() == '"')
        return std::string(raw);
    return "'" + sql_literal_text(stripped) + "'";
}

/**
 * @brief re.sub(r"\s*\n\s*", " ", text).strip().
 */
std::string join_wrapped(std::string_view text) {
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        if (is_space(text[i])) {
            auto end = i;
            bool newline = false;
            while (end < text.size() && is_space(text[end])) {
                newline = newline || text[end] == '\n';
                ++end;
            }
            if (newline)
                out += ' ';
            else
                out.append(text.substr(i, end - i));
            i = end;
            continue;
        }
        out += text[i++];
    }
    return std::string(trim(out));
}

/**
 * @brief re.split(r"\n\s*\n", text): the paragraphs of a body.
 */
std::vector<std::string_view> paragraphs(std::string_view text) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] != '\n') {
            ++i;
            continue;
        }
        auto end = i + 1;
        while (end < text.size() && is_space(text[end]))
            ++end;
        auto last_newline = std::string_view::npos;
        for (auto k = i + 1; k < end; ++k) {
            if (text[k] == '\n')
                last_newline = k;
        }
        if (last_newline == std::string_view::npos) {
            ++i;
            continue;
        }
        out.push_back(text.substr(start, i - start));
        start = last_newline + 1;
        i = start;
    }
    out.push_back(text.substr(start));
    return out;
}

}

/**
 * @brief _column_node_to_dict.
 */
void column_node(load_context& c, value_id out, node_id n) {
    auto& b = c.b;
    const auto& o = c.o;
    b.set(out, "name", o.titles[n.value]);
    set_properties(b, out, o, n, {"default", "default_value"});
    const auto cpp_type = b.find(out, "cpp_type");
    if (truthy(b, b.find(out, "is_enum")) && truthy(b, cpp_type)) {
        const auto type = text_value(b, cpp_type);
        for (const auto key : {"default", "default_value"}) {
            const auto v = b.find(out, key);
            const auto s = std::string(string_of(b, v));
            if (!v.is_none() && s.find("::") == std::string::npos)
                b.set(out, key, type + "::" + std::string(trim(s)));
        }
    } else if (one_of(string_of(b, b.find(out, "type")), sql_string_types)) {
        for (const auto key : {"default", "default_value"}) {
            const auto v = b.find(out, key);
            if (!v.is_none())
                b.set(out, key, sql_quote_default(std::string(string_of(b, v))));
        }
    }
    const auto text = body(o, n);
    const auto parts = paragraphs(text);
    const auto description = join_wrapped(parts.front());
    std::string detail;
    for (std::size_t i = 1; i < parts.size(); ++i) {
        if (i != 1)
            detail += "\n\n";
        detail += join_wrapped(parts[i]);
    }
    if (!description.empty())
        b.set(out, "description", description);
    if (!detail.empty())
        b.set(out, "detail", detail);
    const auto generator = named_block(o, n, "generator");
    if (generator.found)
        b.set(out, "generator_expr", generator.value);
}

/**
 * @brief _soft_fk_validation_node_to_dict.
 */
void soft_fk_node(load_context& c, value_id out, node_id n) {
    auto& b = c.b;
    const auto& o = c.o;
    b.set(out, "column", o.titles[n.value]);
    set_properties(b, out, o, n);
    const auto message = b.find(out, "error_message");
    if (!message.is_none() &&
        b.tables().values.kinds[message] == value_kind::string)
        b.set(out, "error_message",
            sql_literal_text(std::string(string_of(b, message))));
    if (b.find(out, "target_column").is_none())
        b.set(out, "target_column", "id");
    const bool no_validity = truthy(b, b.find(out, "target_no_validity"));
    b.remove(out, "target_no_validity");
    b.set(out, "target_no_validity", no_validity);
    if (string_of(b, b.find(out, "table")) == "ores_iam_accounts_tbl" &&
        string_of(b, b.find(out, "target_column")) == "id" &&
        b.find(out, "exists_function").is_none())
        b.set(out, "exists_function", "ores_iam_account_exists_fn");
    if (const auto r = named_block(o, n, "parent_seed"); r.found)
        b.set(out, "parent_seed_snippet", r.value);
    if (const auto r = named_block(o, n, "parent_seed_includes"); r.found)
        b.set(out, "parent_seed_includes", r.value);
}

namespace {

void typed_row(builder& b, value_id out, const org_row& row,
    bool skip_empty = false) {
    for (const auto& cell : row.cells) {
        if (skip_empty && cell.value.empty())
            continue;
        set_typed(b, out, cell.key, cell.value);
    }
}

void split_csv_trimmed(builder& b, value_id list, std::string_view s) {
    for (;;) {
        const auto comma = s.find(',');
        b.append(list, trim(s.substr(0, comma)));
        if (comma == std::string_view::npos)
            return;
        s.remove_prefix(comma + 1);
    }
}

/**
 * @brief A key a row must have, as r["key"] reads it.
 */
std::string_view required(const org_row& row, std::string_view key) {
    const auto r = row.get(key);
    if (!r.found)
        throw model_error("table row has no " + std::string(key));
    return r.value;
}

std::string feature_name(std::string_view cell) {
    constexpr std::string_view open = "[[id:";
    for (auto at = cell.find(open); at != std::string_view::npos;
         at = cell.find(open, at + 1)) {
        auto i = at + open.size();
        const auto id_start = i;
        while (i < cell.size() && (std::isxdigit(
            static_cast<unsigned char>(cell[i])) || cell[i] == '-'))
            ++i;
        if (i == id_start || cell.substr(i, 2) != "][")
            continue;
        const auto name_start = i + 2;
        const auto close = cell.find(']', name_start);
        if (close == std::string_view::npos || close == name_start ||
            cell.substr(close, 2) != "]]")
            continue;
        return std::string(trim(cell.substr(name_start, close - name_start)));
    }
    return std::string(trim(strip_chars(trim(cell), "=")));
}

}

const std::vector<profile_assignment>& profile_assignments(load_context& c,
    std::string_view slug) {
    std::string file(slug);
    std::ranges::replace(file, '-', '_');
    const auto path = "projects/modeling/variability_" + file + ".org";
    const auto d = c.find(path);
    if (!d)
        throw model_error("unknown profile '" + std::string(slug) + "'");
    auto& cached = c.cache.get().profiles[path];
    if (cached.source == d->source.data() && cached.size == d->source.size())
        return cached.value;
    cached = {d->source.data(), d->source.size(), {}};
    auto& out = cached.value;
    outline o;
    build_outline(d->source, *d->tables, o);
    const auto assignments = section(o, node_id{0}, "Assignments");
    if (assignments.is_none())
        return out;
    for (const auto& row : first_table_rows(o, assignments)) {
        const auto name = feature_name(row.value_or("Feature"));
        const auto v = trim(row.value_or("Value"));
        profile_assignment a;
        a.feature = name;
        if (v == "true" || v == "false") {
            a.is_bool = true;
            a.flag = v == "true";
        } else if (v.starts_with("=") && v.ends_with("=")) {
            a.text = strip_chars(v, "=");
        } else if (!v.empty() && v.find(' ') == std::string_view::npos &&
            v.find('(') == std::string_view::npos) {
            a.text = std::string(v);
        } else {
            continue;
        }
        if (!name.empty())
            out.push_back(std::move(a));
    }
    return out;
}

std::vector<std::string> profile_list(const builder& b, value_id profile) {
    std::vector<std::string> out;
    if (!truthy(b, profile))
        return out;
    auto s = text_value(b, profile);
    std::string_view rest = s;
    for (;;) {
        const auto comma = rest.find(',');
        const auto part = trim(rest.substr(0, comma));
        if (!part.empty())
            out.emplace_back(part);
        if (comma == std::string_view::npos)
            break;
        rest.remove_prefix(comma + 1);
    }
    return out;
}

static bool same(const profile_assignment& a, const profile_assignment& b) {
    return a.is_bool == b.is_bool && a.flag == b.flag && a.text == b.text;
}

static void set_assignment(builder& b, value_id parent, std::string_view key,
    const profile_assignment& a) {
    if (a.is_bool)
        b.set(parent, key, a.flag);
    else
        b.set(parent, key, a.text);
}

/**
 * @brief _profile_namespace_defaults, written into @p out where @p out does
 * not already hold the key.
 */
static void seed_namespace_defaults(load_context& c, value_id profile,
    std::string_view ns, value_id out) {
    auto& b = c.b;
    std::vector<profile_assignment> seen;
    for (const auto& slug : profile_list(b, profile)) {
        for (const auto& a : profile_assignments(c, slug)) {
            const auto feature_ns = namespace_of(a.feature);
            if (!feature_ns || *feature_ns != ns)
                continue;
            const auto it = std::ranges::find(seen, a.feature,
                &profile_assignment::feature);
            if (it != seen.end()) {
                if (!same(*it, a))
                    throw model_error("conflicting profiles on '" +
                        a.feature + "'");
                *it = a;
            } else {
                seen.push_back(a);
            }
        }
    }
    for (const auto& a : seen) {
        if (b.find(out, a.feature).is_none())
            set_assignment(b, out, a.feature, a);
    }
}

void apply_profile(load_context& c, value_id de) {
    auto& b = c.b;
    const auto slugs = profile_list(b, b.find(de, "profile"));
    std::vector<profile_assignment> applied;
    for (const auto& slug : slugs) {
        for (const auto& a : profile_assignments(c, slug)) {
            const auto it = std::ranges::find(applied, a.feature,
                &profile_assignment::feature);
            if (it != applied.end()) {
                if (!same(*it, a))
                    throw model_error("conflicting profiles on '" +
                        a.feature + "'");
                continue;
            }
            applied.push_back(a);
            const auto ns = namespace_of(a.feature);
            if (!ns)
                continue;
            if (ns->empty()) {
                if (b.find(de, a.feature).is_none())
                    set_assignment(b, de, a.feature, a);
                continue;
            }
            const auto sub = b.ensure_object(de, *ns);
            if (b.find(sub, a.feature).is_none())
                set_assignment(b, sub, a.feature, a);
        }
    }
}

static std::vector<value_id> items_of(const builder& b, value_id list) {
    std::vector<value_id> out;
    if (list.is_none())
        return out;
    const auto& v = b.tables().values;
    for (auto i = v.firsts[list]; !i.is_none(); i = v.nexts[i])
        out.push_back(i);
    return out;
}

/**
 * @brief _column_name: column, field or name, the first that is set.
 */
static std::string column_name(const builder& b, value_id column) {
    for (const auto key : {"column", "field", "name"}) {
        if (const auto v = b.find(column, key); truthy(b, v))
            return text_value(b, v);
    }
    return {};
}

/**
 * @brief The columns filter_members gives a member, which are those a
 * list_default_filter may name, after its check of reserved names.
 */
static std::vector<std::string> filter_columns(const builder& b,
    value_id de) {
    std::vector<std::string> declared_names;
    std::vector<value_id> write_columns;
    const auto pk = b.find(de, "primary_key");
    std::vector<value_id> declared;
    if (!pk.is_none())
        declared = items_of(b, b.find(pk, "columns"));
    for (const auto key : {"natural_keys", "columns"}) {
        for (const auto col : items_of(b, b.find(de, key)))
            declared.push_back(col);
    }
    for (const auto col : declared) {
        const auto name = column_name(b, col);
        if (std::ranges::find(declared_names, name) != declared_names.end())
            continue;
        declared_names.push_back(name);
        write_columns.push_back(col);
    }
    std::vector<std::string> relations;
    for (const auto extra : items_of(b, b.find(de, "extra_list_requests"))) {
        if (const auto f = b.find(extra, "filter_column"); truthy(b, f))
            relations.push_back(text_value(b, f));
    }
    if (truthy(b, b.find(de, "has_parent_id"))) {
        const auto presentation = b.find(de, "presentation");
        std::string parent;
        if (const auto v = b.find(de, "parent_id_field"); truthy(b, v))
            parent = text_value(b, v);
        else if (!presentation.is_none()) {
            for (const auto key : {"parent_id_field", "parent_key_field"}) {
                if (const auto v = b.find(presentation, key); truthy(b, v)) {
                    parent = text_value(b, v);
                    break;
                }
            }
        }
        if (!parent.empty() &&
            std::ranges::find(relations, parent) == relations.end())
            relations.push_back(parent);
    }
    std::vector<std::string> filterable;
    for (const auto col : write_columns) {
        const auto f = b.find(col, "filterable");
        if (!f.is_none() &&
            b.tables().values.kinds[f] == value_kind::boolean &&
            b.tables().values.scalars[f] != 0)
            filterable.push_back(column_name(b, col));
    }
    std::vector<std::string> known;
    const auto add = [&](const std::string& name) {
        if (std::ranges::find(known, name) == known.end())
            known.push_back(name);
    };
    std::vector<std::string> members;
    if (const auto v = b.find(de, "list_filter_column"); truthy(b, v)) {
        add(text_value(b, v));
        members.push_back(text_value(b, v));
    }
    for (const auto& r : relations) {
        add(r);
        members.push_back(r);
        members.push_back(r + "_one_of");
    }
    for (const auto& f : filterable) {
        add(f);
        members.push_back(f);
        members.push_back(f + "_one_of");
    }
    const auto keys = pk.is_none() ? std::vector<value_id>{}
        : items_of(b, b.find(pk, "columns"));
    if (keys.size() == 1) {
        const auto key = column_name(b, keys.front());
        add(key);
        members.push_back(key + "_one_of");
    }
    constexpr std::string_view reserved[] = {"offset", "limit", "order",
        "descending", "search", "scope", "as_of", "keys"};
    for (const auto& m : members) {
        if (std::ranges::find(reserved, m) != std::end(reserved))
            throw model_error("filter member '" + m + "' is reserved");
    }
    return known;
}

/**
 * @brief _parse_list_default_filter: <column> = <expression>, where the
 * column is a filter member.
 */
static void list_default_filter(builder& b, value_id de) {
    const auto raw = text_value(b, b.find(de, "list_default_filter"));
    const auto eq = raw.find('=');
    const auto column = std::string(trim(std::string_view(raw).substr(0, eq)));
    const auto expression = eq == std::string::npos ? std::string()
        : std::string(trim(std::string_view(raw).substr(eq + 1)));
    if (eq == std::string::npos || column.empty() || expression.empty())
        throw model_error("list_default_filter must read <column> = "
            "<expression>");
    const auto known = filter_columns(b, de);
    if (std::ranges::find(known, column) == known.end())
        throw model_error("list_default_filter names a column that is not a "
            "filter member");
    const auto out = b.object(de, "list_default_filter");
    b.set(out, "column", column);
    b.set(out, "expression", expression);
}

static void service_find_by_code(builder& b, value_id de) {
    const auto v = b.find(de, "service_find_by_code_column");
    if (v.is_none())
        return;
    std::vector<std::string> columns;
    const auto text = text_value(b, v);
    if (b.tables().values.kinds[v] == value_kind::string) {
        std::string_view rest = text;
        for (;;) {
            rest = trim(rest);
            if (rest.empty())
                break;
            std::size_t end = 0;
            while (end < rest.size() && !is_space(rest[end]))
                ++end;
            columns.emplace_back(rest.substr(0, end));
            rest.remove_prefix(end);
        }
    } else {
        columns.push_back(text);
    }
    b.remove(de, "service_find_by_code_column");
    const auto finder = b.object(de, "service_find_by_code");
    if (columns.empty())
        throw model_error("service_find_by_code_column names no column");
    b.set(finder, "column", columns.front());
    const auto list = b.list(finder, "columns");
    for (const auto& col : columns)
        b.append(list, col);
}

/**
 * @brief _headers_for_types: registered headers whose type occurs as a
 * whole name in one of @p types, in registry order.
 */
static std::vector<std::string> headers_for_types(load_context& c,
    const std::vector<std::string>& types) {
    const auto ident = [](char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '_';
    };
    std::vector<std::string> out;
    for (const auto& [name, header] : custom_type_headers(c)) {
        if (std::ranges::find(out, header) != out.end())
            continue;
        const bool used = std::ranges::any_of(types, [&](const std::string& t) {
            for (auto at = t.find(name); at != std::string::npos;
                 at = t.find(name, at + 1)) {
                if (at > 0 && (ident(t[at - 1]) || t[at - 1] == ':'))
                    continue;
                const auto end = at + name.size();
                if (end < t.size() && ident(t[end]))
                    continue;
                return true;
            }
            return false;
        });
        if (used)
            out.push_back(header);
    }
    return out;
}

void with_registered_headers(load_context& c, value_id includes,
    value_id columns) {
    auto& b = c.b;
    std::vector<std::string> types;
    if (!columns.is_none()) {
        const auto& v = b.tables().values;
        for (auto col = v.firsts[columns]; !col.is_none(); col = v.nexts[col]) {
            const auto t = b.find(col, "cpp_type");
            if (truthy(b, t))
                types.push_back(text_value(b, t));
        }
    }
    if (types.empty())
        return;
    const auto domain = b.ensure_list(includes, "domain");
    for (const auto& header : headers_for_types(c, types)) {
        bool present = false;
        const auto& v = b.tables().values;
        for (auto h = v.firsts[domain]; !h.is_none(); h = v.nexts[h])
            present = present || string_of(b, h) == header;
        if (!present)
            b.append(domain, header);
    }
}

void cpp_includes(load_context& c, value_id cpp_out, node_id dom, node_id ent,
    value_id columns) {
    auto& b = c.b;
    const auto& o = c.o;
    if (dom.is_none() && ent.is_none())
        return;
    const auto includes = b.object(cpp_out, "includes");
    const auto domain = b.list(includes, "domain");
    if (!dom.is_none())
        includes_from_named_block(c, dom, domain);
    const auto entity = b.list(includes, "entity");
    if (!ent.is_none()) {
        const auto code = named_block(o, ent, "includes");
        for (const auto raw : lines_of(code.value)) {
            const auto line = trim(raw);
            if (!line.starts_with("#include"))
                continue;
            const auto token = trim(line.substr(8));
            if (!one_of(token, entity_header_standard_includes))
                b.append(entity, token);
        }
    }
    with_registered_headers(c, includes, columns);
}

artefact_section artefact_columns(load_context& c) {
    const auto& o = c.o;
    artefact_section a;
    a.node = section(o, node_id{0}, "Artefact columns");
    if (a.node.is_none())
        return a;
    a.columns = children(o, a.node);
    if (a.columns.empty())
        throw model_error("'* Artefact columns' declares no column");
    if (!tables_of(o, a.node).empty()) {
        const auto rows = first_table_rows(o, a.node);
        if (!rows.empty() && !rows.front().get("expression").found)
            throw model_error("artefact check table has no expression");
        for (const auto& r : rows) {
            if (!r.value_or("expression").empty())
                a.checks.push_back(r.value_or("expression"));
        }
    }
    const auto key = property(o, a.node, "key");
    a.key = key.value.empty() ? o.titles[a.columns.front().value] : key.value;
    return a;
}

void presentation_drawer(load_context& c, value_id out, node_id drawer,
    value_id profile) {
    auto& b = c.b;
    const auto& o = c.o;
    set_properties(b, out, o, drawer);
    seed_namespace_defaults(c, profile, "presentation", out);

    const auto rows_under = [&](std::string_view title) {
        return own_table_rows(o, section(o, drawer, title));
    };
    const auto has = [&](std::string_view title) {
        return !section(o, drawer, title).is_none();
    };

    if (has("Detail fields")) {
        const auto list = b.list(out, "detail_fields");
        for (const auto& row : rows_under("Detail fields")) {
            const auto f = b.append_object(list);
            typed_row(b, f, row, true);
            const auto raw = b.find(f, "combo_values");
            if (string_of(b, b.find(f, "type")) == "static_combo" &&
                !raw.is_none() &&
                b.tables().values.kinds[raw] == value_kind::string &&
                !string_of(b, raw).empty()) {
                const std::string text(string_of(b, raw));
                const auto values = b.list(f, "combo_values");
                std::string_view rest = text;
                for (;;) {
                    const auto comma = rest.find(',');
                    const auto v = trim(rest.substr(0, comma));
                    if (!v.empty()) {
                        const auto item = b.append_object(values);
                        b.set(item, "label", v);
                        b.set(item, "value", v);
                    }
                    if (comma == std::string_view::npos)
                        break;
                    rest.remove_prefix(comma + 1);
                }
            }
        }
    }
    if (has("Columns")) {
        const auto list = b.list(out, "columns");
        const auto preview = b.list(out, "import_preview_columns");
        for (const auto& row : rows_under("Columns")) {
            const auto entry = b.append_object(list);
            for (const auto& cell : row.cells) {
                if (cell.key == "type") {
                    const auto low = to_lower(cell.value);
                    if (low == "string") b.set(entry, "is_string", true);
                    else if (low == "int") b.set(entry, "is_int", true);
                    else if (low == "bool" || low == "boolean")
                        b.set(entry, "is_bool", true);
                    else if (low == "double") b.set(entry, "is_double", true);
                    else if (low == "timestamp")
                        b.set(entry, "is_timestamp", true);
                    else if (low == "uuid") b.set(entry, "is_uuid", true);
                    else if (low == "date") b.set(entry, "is_date", true);
                    continue;
                }
                set_typed(b, entry, cell.key, cell.value);
            }
            if (truthy(b, b.find(entry, "import_preview")))
                b.append_copy(preview, entry);
        }
    }
    bool has_icon_columns = false;
    if (has("Icon columns")) {
        const auto list = b.list(out, "icon_columns");
        bool pair = false;
        for (const auto& row : rows_under("Icon columns")) {
            const auto entry = b.append_object(list);
            typed_row(b, entry, row);
            const bool field2 = truthy(b, b.find(entry, "field2"));
            b.set(entry, "has_field2", field2);
            if (b.find(entry, "is_pair").is_none())
                b.set(entry, "is_pair", field2);
            pair = pair || truthy(b, b.find(entry, "is_pair"));
        }
        has_icon_columns = truthy(b, list);
        b.set(out, "has_icon_columns", has_icon_columns);
        b.set(out, "has_pair_icon_column", pair);
    }
    bool generate = false;
    if (has("Setting-gated actions")) {
        const auto list = b.list(out, "setting_gated_actions");
        for (const auto& row : rows_under("Setting-gated actions")) {
            const auto entry = b.append_object(list);
            typed_row(b, entry, row);
            generate = generate ||
                string_of(b, b.find(entry, "action")) == "generateAction";
        }
        b.set(out, "has_setting_gated_actions", truthy(b, list));
    }
    b.set(out, "has_generate_action", generate);
    b.set(out, "has_toolbar", truthy(b, b.find(out, "has_toolbar")) ||
        truthy(b, b.find(out, "has_version_navigation")) || generate);
    if (has("Related entity shortcuts")) {
        const auto list = b.list(out, "related_entity_shortcuts");
        for (const auto& row : rows_under("Related entity shortcuts"))
            typed_row(b, b.append_object(list), row);
        b.set(out, "has_related_entity_shortcuts", truthy(b, list));
    }
    const bool flag_icon = truthy(b, b.find(out, "flag_icon_column"));
    b.set(out, "has_flag_icon", flag_icon);
    if (!truthy(b, b.find(out, "key_flag_field")) &&
        truthy(b, b.find(out, "flag_inline_widget")))
        b.set_copy(out, "key_flag_field", b.find(out, "flag_inline_widget"));
    if (!truthy(b, b.find(out, "key_flag_accessor")) &&
        truthy(b, b.find(out, "flag_accessor")))
        b.set_copy(out, "key_flag_accessor", b.find(out, "flag_accessor"));
    const bool key_flag = truthy(b, b.find(out, "key_flag_field"));
    b.set(out, "has_key_flag_icon", key_flag);
    b.set(out, "has_any_flag_icon", flag_icon || has_icon_columns);
    bool combo_flag = false;
    bool dynamic_combo = false;
    if (const auto fields = b.find(out, "detail_fields"); !fields.is_none()) {
        const auto& v = b.tables().values;
        for (auto f = v.firsts[fields]; !f.is_none(); f = v.nexts[f]) {
            combo_flag = combo_flag || truthy(b, b.find(f, "flag_source"));
            dynamic_combo = dynamic_combo ||
                truthy(b, b.find(f, "combo_domain_type"));
        }
    }
    b.set(out, "has_combo_flag_source", combo_flag);
    b.set(out, "has_dynamic_combo_helper_fields", dynamic_combo);
    b.set(out, "needs_image_cache",
        flag_icon || key_flag || has_icon_columns || combo_flag);
}

void load_entity(load_context& c, value_id root) {
    auto& b = c.b;
    const auto& o = c.o;
    const node_id document{0};
    ensure_profile_binding(o);
    reject_junction_only_flags(o, "domain_entity", true);
    const auto de = b.object(root, "domain_entity");

    for (const auto key : {"brief", "entity_singular", "entity_plural",
        "entity_title", "http_area", "web_resource", "web_list_by",
        "web_area", "web_writes", "web_parent_filter",
        "web_read_only_because", "web_key_read", "web_classification",
        "web_classification_topic"}) {
        if (const auto r = frontmatter(o, key); r.found)
            b.set(de, key, r.value);
    }
    for (const auto key : {"has_tenant_id", "image_id",
        "has_artefact_insert_fn", "web_read_only", "web_system_owned",
        "web_classification_read_only"}) {
        if (const auto r = frontmatter(o, key); r.found)
            set_typed(b, de, key, r.value);
    }
    if (const auto r = frontmatter(o, "coding_scheme"); r.found)
        b.set(de, "coding_scheme", r.value);
    if (const auto text = body(o, document); !text.empty())
        b.set(de, "description", text);
    {
        lookup id;
        for (std::size_t i = 0; i < o.file_property_keys.size(); ++i) {
            if (iequals(o.file_property_keys[i], "ID"))
                id = {true, o.file_property_values[i]};
        }
        if (!id.value.empty())
            b.set(de, "entity_org_id", id.value);
    }

    if (const auto flags = section(o, document, "Flags"); !flags.is_none()) {
        set_properties(b, de, o, flags);
        service_find_by_code(b, de);
    }

    if (const auto cols = section(o, document, "Columns"); !cols.is_none()) {
        std::vector<node_id> pk, nk, plain;
        for (const auto child : children(o, cols)) {
            const auto is_flag = [&](std::string_view key) {
                const auto r = property(o, child, key);
                return iequals(r.found ? r.value : "false", "true");
            };
            const bool is_pk = is_flag("primary_key");
            const bool is_nk = is_flag("natural_key");
            if (is_pk && is_nk)
                throw model_error("column is both primary and natural key");
            if ((is_pk || is_nk) && is_flag("is_value_type"))
                throw model_error("key column opts in as a value type");
            (is_pk ? pk : is_nk ? nk : plain).push_back(child);
        }
        const auto fill = [&](value_id list, const std::vector<node_id>& nodes,
            bool key) {
            for (const auto n : nodes) {
                const auto d = b.append_object(list);
                column_node(c, d, n);
                if (key) {
                    const auto name = b.find(d, "name");
                    b.set_copy(d, "column", name);
                    b.remove(d, "name");
                }
                b.remove(d, "primary_key");
                b.remove(d, "natural_key");
            }
        };
        if (!pk.empty()) {
            const auto fields = b.detached_list();
            fill(fields, pk, true);
            const auto key = b.object(de, "primary_key");
            const auto& v = b.tables().values;
            for (auto f = v.firsts[v.firsts[fields]]; !f.is_none();
                 f = b.tables().values.nexts[f]) {
                const std::string k(types::text_of(b.tables().texts,
                    b.tables().values.keys[f]));
                b.set_copy(key, k, f);
            }
            b.set_copy(key, "columns", fields);
        }
        if (!nk.empty())
            fill(b.list(de, "natural_keys"), nk, true);
        fill(b.list(de, "columns"), plain, false);
    }

    const auto sql = section(o, document, "SQL");
    if (!sql.is_none()) {
        const auto out = b.object(de, "sql");
        set_properties(b, out, o, sql);
    }
    if (const auto repo = section(o, document, "Repository");
        !repo.is_none())
        set_properties(b, b.object(de, "repository"), o, repo);

    const auto fk_section = section(o, document, "Foreign keys");
    if (!children(o, fk_section).empty()) {
        const auto list = b.list(de, "foreign_keys");
        for (const auto n : children(o, fk_section)) {
            const auto fk = b.append_object(list);
            soft_fk_node(c, fk, n);
            if (truthy(b, b.find(fk, "bump_parent_version"))) {
                auto table = text_value(b, b.find(fk, "table"));
                if (table.ends_with("_tbl"))
                    table = table.substr(0, table.size() - 4) +
                        "_touch_version_fn";
                b.set(fk, "touch_function", table);
            }
        }
    }

    const auto cascade = section(o, document, "Delete cascade");
    if (!children(o, cascade).empty()) {
        const auto list = b.list(de, "delete_cascade");
        for (const auto n : children(o, cascade)) {
            const auto fk = b.detached_object();
            soft_fk_node(c, fk, n);
            if (!truthy(b, b.find(fk, "table")))
                continue;
            const auto item = b.append_object(list);
            b.set_copy(item, "table", b.find(fk, "table"));
            b.set_copy(item, "column", b.find(fk, "column"));
        }
        b.set(de, "has_delete_cascade", truthy(b, list));
    }

    if (!sql.is_none()) {
        if (const auto flags = section(o, sql, "Flags"); !flags.is_none())
            set_properties(b, b.object(de, "sql"), o, flags);
        const auto expressions = [&](std::string_view title,
            std::string_view key) {
            const auto n = section(o, sql, title);
            if (n.is_none() || tables_of(o, n).empty())
                return;
            std::vector<std::string_view> values;
            for (const auto& r : first_table_rows(o, n)) {
                const auto e = r.value_or("expression");
                if (!e.empty())
                    values.push_back(e);
            }
            if (values.empty())
                return;
            const auto list = b.list(b.ensure_object(de, "sql"), key);
            for (const auto v : values)
                b.append(list, v);
        };
        expressions("Checks", "extra_checks");
        expressions("Delete sets", "extra_delete_sets");
        const auto bnk = section(o, sql, "Bitemporal natural keys");
        if (!bnk.is_none() && !tables_of(o, bnk).empty()) {
            const auto list = b.list(b.ensure_object(de, "sql"),
                "bitemporal_natural_keys");
            for (const auto& r : first_table_rows(o, bnk)) {
                if (r.value_or("column").empty())
                    continue;
                const auto item = b.append_object(list);
                b.set(item, "column", required(r, "column"));
                set_typed(b, item, "is_nullable",
                    r.value_or("nullable", "false"));
            }
        }
        const auto sql_value = b.find(de, "sql");
        if (!sql_value.is_none() &&
            string_of(b, b.find(sql_value, "bitemporal_trigger")) ==
                "soft_update_delete" &&
            !truthy(b, b.find(sql_value, "bitemporal_natural_keys")))
            throw model_error("soft_update_delete needs bitemporal natural "
                "keys");
        const auto raw_struct = [&](std::string_view title,
            std::string_view key, std::string_view message) {
            const auto n = section(o, sql, title);
            const auto props = properties(o, n);
            if (props.empty())
                return;
            const auto out = b.object(b.ensure_object(de, "sql"), key);
            for (const auto& p : props) {
                const auto k = to_lower(p.key);
                if (k == message)
                    b.set(out, k, sql_literal_text(p.value));
                else
                    b.set(out, k, p.value);
            }
        };
        raw_struct("Party id from book id", "party_id_from_book_id",
            "book_error_message");
        raw_struct("Party id from parent", "party_id_from_parent",
            "parent_error_message");
        const auto transition = section(o, sql, "Status transition");
        if (const auto r = named_block(o, transition, "amend_activity");
            r.found)
            b.set(b.object(de, "amend_activity"), "snippet", r.value);
        const auto pins = section(o, sql, "Pinned keys");
        if (!pins.is_none() && !tables_of(o, pins).empty()) {
            const auto list = b.list(de, "pinned_keys");
            for (const auto& r : first_table_rows(o, pins)) {
                if (r.value_or("name").empty())
                    continue;
                const auto item = b.append_object(list);
                b.set(item, "name", required(r, "name"));
                split_csv_trimmed(b, b.list(item, "columns"),
                    required(r, "columns"));
                b.set(item, "table", required(r, "table"));
                split_csv_trimmed(b, b.list(item, "target_columns"),
                    required(r, "target_columns"));
                b.set(item, "error_message",
                    sql_literal_text(required(r, "error_message")));
            }
        }
        const auto view = section(o, sql, "View");
        if (const auto r = named_block(o, view, "select"); r.found) {
            auto select = trim(r.value);
            while (!select.empty() && select.back() == ';')
                select.remove_suffix(1);
            b.set(b.ensure_object(de, "sql"), "view_select", select);
        }
        const auto indexes = section(o, sql, "Indexes");
        if (!indexes.is_none() && !tables_of(o, indexes).empty()) {
            const auto list = b.list(de, "indexes");
            for (const auto& r : first_table_rows(o, indexes)) {
                if (r.value_or("name").empty())
                    continue;
                const auto item = b.append_object(list);
                b.set(item, "name", required(r, "name"));
                b.set(item, "columns", r.value_or("columns"));
                set_typed(b, item, "unique", r.value_or("unique", "false"));
                set_typed(b, item, "current_only",
                    r.value_or("current_only", "false"));
                if (r.value_or("where_extra").empty())
                    b.set_null(item, "where_extra");
                else
                    b.set(item, "where_extra", r.value_or("where_extra"));
                set_typed(b, item, "nulls_not_distinct",
                    r.value_or("nulls_not_distinct", "false"));
                if (r.value_or("using").empty())
                    b.set_null(item, "using");
                else
                    b.set(item, "using", r.value_or("using"));
            }
        }
    }

    if (const auto artefact = section(o, document, "Artefact indexes");
        !artefact.is_none()) {
        const auto list = b.list(de, "artefact_indexes");
        for (const auto n : children(o, artefact)) {
            const auto entry = b.append_object(list);
            b.set(entry, "name", o.titles[n.value]);
            set_properties(b, entry, o, n, {"columns"});
        }
    }

    if (const auto ac = artefact_columns(c); !ac.node.is_none()) {
        if (truthy(b, b.find(de, "has_artefact_insert_fn")))
            throw model_error("both '* Artefact columns' and "
                ":has_artefact_insert_fn:");
        const auto list = b.list(de, "artefact_columns");
        for (const auto n : ac.columns)
            column_node(c, b.append_object(list), n);
        b.set(de, "artefact_key_column", ac.key);
        if (!ac.checks.empty()) {
            const auto out = b.list(de, "artefact_checks");
            for (const auto ch : ac.checks)
                b.append(out, ch);
        }
    }

    if (const auto fks = b.find(de, "foreign_keys"); !fks.is_none()) {
        bool as_of = false;
        const auto& v = b.tables().values;
        for (auto f = v.firsts[fks]; !f.is_none(); f = v.nexts[f])
            as_of = as_of || truthy(b, b.find(f, "list_by_as_of"));
        if (as_of)
            b.set(b.ensure_object(de, "sql"), "has_list_by_as_of", true);
    }

    if (const auto vfn = section(o, document, "Validation function");
        !vfn.is_none())
        set_properties(b, b.object(de, "validation_fn"), o, vfn,
            {"default", "default_value"});

    if (const auto insert = section(o, document, "Insert trigger");
        !insert.is_none()) {
        const auto validations = section(o, insert, "Validations");
        if (!validations.is_none()) {
            const auto trigger = b.object(de, "insert_trigger");
            const auto list = b.list(trigger, "validations");
            const auto columns = b.find(de, "columns");
            for (const auto& r : first_table_rows(o, validations)) {
                const auto item = b.append_object(list);
                for (const auto& cell : r.cells)
                    b.set(item, cell.key, cell.value);
                const auto column = r.get("column");
                value_id nullable = value_id::none();
                if (column.found && !columns.is_none()) {
                    const auto& v = b.tables().values;
                    for (auto col = v.firsts[columns]; !col.is_none();
                         col = v.nexts[col]) {
                        if (string_of(b, b.find(col, "name")) == column.value)
                            nullable = b.find(col, "nullable");
                    }
                }
                if (nullable.is_none())
                    b.set(item, "nullable", false);
                else
                    b.set_copy(item, "nullable", nullable);
            }
        }
    }

    if (const auto checks = section(o, document, "Check constraints");
        !checks.is_none()) {
        const auto list = b.list(de, "check_constraints");
        for (const auto n : children(o, checks)) {
            const auto entry = b.append_object(list);
            for (const auto& p : properties(o, n))
                b.set(entry, to_lower(p.key), p.value);
        }
    }

    if (const auto idx = section(o, document, "Indexes"); !idx.is_none()) {
        const auto list = b.list(de, "indexes");
        for (const auto n : children(o, idx)) {
            const auto entry = b.append_object(list);
            b.set(entry, "name", o.titles[n.value]);
            set_properties(b, entry, o, n);
        }
    }

    if (const auto cpp = section(o, document, "C++"); !cpp.is_none()) {
        if (const auto flags = section(o, cpp, "Flags"); !flags.is_none()) {
            set_properties(b, de, o, flags);
            if (!b.find(de, "service_find_by_code_column").is_none()) {
                service_find_by_code(b, de);
                const auto parent =
                    b.find(de, "service_find_by_code_parent_column");
                if (!parent.is_none()) {
                    b.set_copy(b.find(de, "service_find_by_code"),
                        "parent_column", parent);
                    b.remove(de, "service_find_by_code_parent_column");
                }
            }
            if (!b.find(de, "list_default_filter").is_none())
                list_default_filter(b, de);
        }
        if (const auto repo = section(o, cpp, "Repository"); !repo.is_none())
            set_properties(b, b.object(de, "repository"), o, repo);

        const auto cpp_out = b.object(de, "cpp");
        cpp_includes(c, cpp_out, section(o, cpp, "Domain includes"),
            section(o, cpp, "Entity includes"), b.find(de, "columns"));
        if (const auto conv = section(o, cpp, "Conventions"); !conv.is_none())
            set_properties(b, cpp_out, o, conv);
        const auto dg = section(o, cpp, "Domain groups");
        if (!dg.is_none() && !tables_of(o, dg).empty()) {
            const auto list = b.list(de, "domain_groups");
            for (const auto& r : first_table_rows(o, dg)) {
                if (r.value_or("member").empty() ||
                    r.value_or("field_group").empty())
                    continue;
                const auto item = b.append_object(list);
                b.set(item, "member", r.value_or("member"));
                b.set(item, "field_group", r.value_or("field_group"));
            }
        }
        if (const auto td = section(o, cpp, "Table display"); !td.is_none()) {
            const auto list = b.list(cpp_out, "table_display");
            for (const auto& r : own_table_rows(o, td)) {
                const auto item = b.append_object(list);
                b.set(item, "column", r.value_or("column"));
                b.set(item, "header", r.value_or("header"));
            }
        }
        const auto iterator = b.find(cpp_out, "iterator_var");
        if (iterator.is_none() || trim(text_value(b, iterator)).empty())
            b.set(cpp_out, "iterator_var", "e");

        if (const auto drawer = section(o, cpp, "Presentation");
            !drawer.is_none())
            presentation_drawer(c, b.object(de, "presentation"), drawer,
                b.find(de, "profile"));

        if (const auto cm = section(o, cpp, "Custom repository methods");
            !cm.is_none()) {
            const auto list = b.list(de, "custom_repository_methods");
            for (const auto n : children(o, cm)) {
                const auto method = b.append_object(list);
                b.set(method, "name", o.titles[n.value]);
                if (const auto id = org_id(o, n); !id.value.empty())
                    b.set(method, "id", id.value);
                b.set(method, "description", body(o, n));
                for (std::size_t i = 0; i < o.block_nodes.size(); ++i) {
                    if (o.block_nodes[i] == n && !o.block_names[i].empty())
                        b.set(method, o.block_names[i], o.block_codes[i]);
                }
            }
        }
    }

    collect_implementations(c, de, "implementations");
    apply_profile(c, de);

    if (!section(o, document, "Messages").is_none() &&
        !children(o, section(o, document, "Messages")).empty()) {
        const auto impls = b.find(de, "implementations");
        if (!impls.is_none() &&
            !b.find(impls, protocol_messages_paste_kind).is_none())
            throw model_error("* Messages and a protocol paste block");
        declared_messages(c, b.list(de, "declared_messages"));
    }

    resolve_domain_group_fields(c, de);
    validate_entity(c, de);
}

}

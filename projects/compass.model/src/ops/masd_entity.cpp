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
#include "compass.model/ops/masd_entity.hpp"

#include <algorithm>
#include <span>
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::key_kind;
using types::written_by;

constexpr std::string_view flag_keys[] = {"product", "component", "schema",
    "profile", "row_type"};

constexpr std::string_view column_keys[] = {"type", "cpp_type", "nullable",
    "default", "primary_key", "key_kind", "written_by", "computed",
    "generated", "values", "enum", "member", "unique"};

constexpr std::string_view foreign_key_keys[] = {"references", "on_delete"};

constexpr std::string_view index_headers[] = {"name", "columns", "unique",
    "where", "using", "opclass"};

constexpr std::string_view sql_sections[] = {"Indexes", "Checks", "Unique"};

constexpr std::string_view schema_flag_keys[] = {"product", "component"};

constexpr std::string_view message_keys[] = {"response", "http_method",
    "http_path", "tool", "area", "entity", "writes", "destructive",
    "idempotent", "summary", "caller", "max_body_size"};

constexpr std::string_view field_keys[] = {"cpp_type", "default", "http_in",
    "http_name", "required", "encoding", "values"};

constexpr std::string_view http_methods[] = {"get", "post", "put", "patch",
    "delete"};

/**
 * @brief The features a profile of a masd.codegen.entity may assign.
 */
constexpr std::string_view profile_features[] = {"retained"};

std::string listed(std::span<const std::string_view> names) {
    std::string out;
    for (const auto n : names)
        out += (out.empty() ? "" : ", ") + std::string(n);
    return out;
}

void require_known(std::string_view key, std::span<const std::string_view> known,
    std::string_view where) {
    if (std::ranges::find(known, key) == known.end())
        throw model_error(std::string(where) + " has the property '" +
            std::string(key) + "', which a masd.codegen model does not "
            "have; it has " + listed(known));
}

types::http_in http_in_of(std::string_view value, std::string_view where) {
    if (value == "path")
        return types::http_in::path;
    if (value == "query")
        return types::http_in::query;
    if (value == "body")
        return types::http_in::body;
    throw model_error(std::string(where) + "'s :http_in: is '" +
        std::string(value) + "'; it is path, query or body");
}

bool flag(std::string_view value, std::string_view where) {
    const auto v = trim(value);
    if (v == "true")
        return true;
    if (v == "false")
        return false;
    throw model_error(std::string(where) + " is '" + std::string(v) +
        "', which is neither true nor false");
}

std::vector<std::string> words(std::string_view s, char separator = ' ') {
    std::vector<std::string> out;
    while (!s.empty()) {
        const auto at = separator == ' '
            ? s.find_first_of(" \t") : s.find(separator);
        const auto word = trim(s.substr(0, at));
        if (!word.empty())
            out.emplace_back(word);
        if (at == std::string_view::npos)
            break;
        s.remove_prefix(at + 1);
    }
    return out;
}

key_kind key_kind_of(std::string_view v, std::string_view where) {
    constexpr std::pair<std::string_view, key_kind> kinds[] = {
        {"natural", key_kind::natural}, {"content_hash", key_kind::content_hash},
        {"owner", key_kind::owner}, {"ord", key_kind::ord},
        {"identity", key_kind::identity}};
    for (const auto& [name, kind] : kinds)
        if (name == v)
            return kind;
    throw model_error(std::string(where) + " has the key_kind '" +
        std::string(v) + "'; a key is natural, content_hash, owner, ord or "
        "identity");
}

/**
 * @brief The feature a cell of an Assignments table names: the
 * description of a link, or the cell without its verbatim markers.
 */
std::string_view feature_of(std::string_view cell) {
    cell = trim(cell);
    if (cell.starts_with("[[")) {
        const auto open = cell.find("][");
        const auto close = cell.rfind("]]");
        if (open != std::string_view::npos && close != std::string_view::npos &&
            close > open)
            return trim(cell.substr(open + 2, close - open - 2));
    }
    while (cell.starts_with('='))
        cell.remove_prefix(1);
    while (cell.ends_with('='))
        cell.remove_suffix(1);
    return cell;
}

void apply_profile(types::masd_entity& e, const document_finder& find) {
    if (e.profile.empty())
        return;
    std::string file = e.profile;
    std::ranges::replace(file, '-', '_');
    const auto path = "projects/modeling/variability_" + file + ".org";
    const auto d = find(path);
    if (!d)
        throw model_error("unknown profile '" + e.profile + "': no page " +
            path);
    outline o;
    build_outline(d->source, *d->tables, o);
    for (const auto& row : first_table_rows(o, section(o, node_id{0},
             "Assignments"))) {
        const auto feature = feature_of(row.value_or("Feature"));
        require_known(feature, profile_features,
            "the profile '" + e.profile + "'");
        if (feature == "retained")
            e.retained = flag(row.value_or("Value"),
                "the profile's retained");
    }
}

types::masd_column column_of(const outline& o, node_id n) {
    types::masd_column c;
    c.name = std::string(trim(o.titles[n.value]));
    c.member = c.name;
    const auto where = "the column '" + c.name + "'";
    for (const auto& [key, raw] : properties(o, n)) {
        require_known(key, column_keys, where);
        const auto value = trim(raw);
        const auto at = where + "'s " + std::string(key);
        if (key == "type") c.sql_type = value;
        else if (key == "cpp_type") c.cpp_type = value;
        else if (key == "default") c.default_value = value;
        else if (key == "generated") c.generated = value;
        else if (key == "enum") c.enum_name = value;
        else if (key == "member") c.member = value;
        else if (key == "values") c.values = words(value);
        else if (key == "nullable") c.nullable = flag(value, at);
        else if (key == "primary_key") c.primary_key = flag(value, at);
        else if (key == "unique") c.unique = flag(value, at);
        else if (key == "key_kind") c.key = key_kind_of(value, where);
        else if (key == "written_by" || key == "computed") {
            if (value == "database" && key == "written_by")
                c.writer = written_by::database;
            else if (value == "writer")
                c.writer = written_by::writer;
            else
                throw model_error(at + " is '" + std::string(value) +
                    "'; it is " + (key == "computed" ? "writer"
                        : "database or writer"));
        }
    }
    if (c.sql_type.empty())
        throw model_error(where + " states no type");
    if (c.key != key_kind::none && !c.primary_key)
        throw model_error(where + " has a key_kind but is not a primary key");
    if (!c.values.empty() && c.sql_type != "text")
        throw model_error(where + " has values but is not text");
    if (!c.generated.empty())
        c.writer = written_by::database;
    return c;
}

void require_column(const types::masd_entity& e, std::string_view name,
    std::string_view where) {
    if (std::ranges::find(e.columns, name, &types::masd_column::name) ==
        e.columns.end())
        throw model_error(std::string(where) + " names the column '" +
            std::string(name) + "', which the model does not have");
}

void require_headers(const std::vector<org_row>& rows,
    std::span<const std::string_view> known, std::string_view where) {
    for (const auto& row : rows)
        for (const auto& cell : row.cells)
            if (std::ranges::find(known, trim(cell.key)) == known.end())
                throw model_error(std::string(where) + " has the column '" +
                    std::string(cell.key) + "'; it has " + listed(known));
}

void read_sql(const outline& o, types::masd_entity& e) {
    const auto sql = section(o, node_id{0}, "SQL");
    for (const auto n : children(o, sql)) {
        const auto title = trim(o.titles[n.value]);
        if (std::ranges::find(sql_sections, title) == std::end(sql_sections))
            throw model_error("* SQL has the section '" + std::string(title) +
                "'; it has " + listed(sql_sections));
        const auto rows = own_table_rows(o, n);
        if (title == "Indexes") {
            require_headers(rows, index_headers, "the Indexes table");
            for (const auto& row : rows) {
                types::masd_index i;
                i.name = std::string(trim(row.value_or("name")));
                i.columns = words(row.value_or("columns"), ',');
                i.unique = flag(row.value_or("unique", "false"),
                    "the index '" + i.name + "''s unique");
                i.method = std::string(trim(row.value_or("using")));
                i.opclass = std::string(trim(row.value_or("opclass")));
                i.where = std::string(trim(row.value_or("where")));
                for (const auto& c : i.columns)
                    require_column(e, c, "the index '" + i.name + "'");
                e.indexes.push_back(std::move(i));
            }
        } else if (title == "Checks") {
            constexpr std::string_view headers[] = {"expression"};
            require_headers(rows, headers, "the Checks table");
            for (const auto& row : rows)
                e.checks.emplace_back(trim(row.value_or("expression")));
        } else {
            constexpr std::string_view headers[] = {"columns"};
            require_headers(rows, headers, "the Unique table");
            for (const auto& row : rows) {
                auto columns = words(row.value_or("columns"), ',');
                for (const auto& c : columns)
                    require_column(e, c, "a unique constraint");
                e.uniques.push_back(std::move(columns));
            }
        }
    }
}

}

types::masd_entity read_masd_entity(const document& d,
    const document_finder& find) {
    outline o;
    build_outline(d.source, *d.tables, o);
    types::masd_entity e;
    for (const auto& [key, value] : properties(o, section(o, node_id{0},
             "Flags"))) {
        require_known(key, flag_keys, "* Flags");
        const std::string v(trim(value));
        if (key == "product") e.product = v;
        else if (key == "component") e.component = v;
        else if (key == "schema") e.schema = v;
        else if (key == "profile") e.profile = v;
        else e.row_type = v;
    }
    e.entity = std::string(trim(frontmatter(o, "entity_singular").value));
    e.brief = std::string(trim(frontmatter(o, "brief").value));
    if (e.entity.empty())
        throw model_error("the model states no #+entity_singular:");
    if (e.schema.empty())
        throw model_error("* Flags states no schema");
    if (e.row_type.empty())
        e.row_type = e.entity + "_row";
    apply_profile(e, find);

    for (const auto n : children(o, section(o, node_id{0}, "Columns"))) {
        auto c = column_of(o, n);
        if (c.enum_name.empty() && !c.values.empty())
            c.enum_name = e.entity + "_" + c.name;
        if (std::ranges::find(e.columns, c.name, &types::masd_column::name) !=
            e.columns.end())
            throw model_error("the column '" + c.name + "' is stated twice");
        e.columns.push_back(std::move(c));
    }
    if (e.columns.empty())
        throw model_error("the model has no * Columns");

    for (const auto n : children(o, section(o, node_id{0}, "Foreign keys"))) {
        types::masd_foreign_key f;
        f.column = std::string(trim(o.titles[n.value]));
        const auto where = "the foreign key '" + f.column + "'";
        for (const auto& [key, value] : properties(o, n)) {
            require_known(key, foreign_key_keys, where);
            if (key == "references") {
                f.references = std::string(trim(value));
            } else if (trim(value) == "cascade") {
                f.cascade = true;
            } else {
                throw model_error(where + "'s on_delete is '" +
                    std::string(trim(value)) + "'; it is cascade");
            }
        }
        if (f.references.empty())
            throw model_error(where + " states no references");
        require_column(e, f.column, where);
        e.foreign_keys.push_back(std::move(f));
    }
    read_sql(o, e);
    return e;
}

types::masd_schema read_masd_schema(const document& d) {
    outline o;
    build_outline(d.source, *d.tables, o);
    types::masd_schema s;
    for (const auto& [key, value] : properties(o, section(o, node_id{0},
             "Flags"))) {
        require_known(key, schema_flag_keys, "* Flags");
        (key == "product" ? s.product : s.component) = trim(value);
    }
    const auto schemas = own_table_rows(o, section(o, node_id{0},
        "Schemas"));
    constexpr std::string_view headers[] = {"name", "description"};
    require_headers(schemas, headers, "the Schemas table");
    for (const auto& row : schemas)
        s.schemas.emplace_back(trim(row.value_or("name")));
    if (s.schemas.empty())
        throw model_error("the model names no schema under * Schemas");

    const auto functions = section(o, node_id{0}, "Functions");
    if (!functions.is_none()) {
        const auto end = o.subtree_ends[functions.value];
        for (std::size_t i = 0; i < o.block_nodes.size(); ++i)
            if (o.block_nodes[i] >= functions && o.block_nodes[i] < end &&
                o.block_languages[i] == "sql")
                s.functions.emplace_back(o.block_codes[i]);
    }
    return s;
}

types::masd_component read_masd_component(const document& d) {
    outline o;
    build_outline(d.source, *d.tables, o);
    types::masd_component m;
    for (const auto& [key, value] : properties(o, section(o, node_id{0},
             "Flags"))) {
        require_known(key, schema_flag_keys, "* Flags");
        (key == "product" ? m.product : m.component) = trim(value);
    }
    if (m.product.empty() || m.component.empty())
        throw model_error("a component model states its product and "
            "component under * Flags");
    return m;
}

types::masd_operation read_masd_operation(const document& d) {
    outline o;
    build_outline(d.source, *d.tables, o);
    types::masd_operation m;
    m.component = trim(frontmatter(o, "component").value);
    m.entity = trim(frontmatter(o, "entity_singular").value);
    m.ns = trim(frontmatter(o, "namespace").value);
    if (m.component.empty() || m.entity.empty() || m.ns.empty())
        throw model_error("an operation model states #+component:, "
            "#+entity_singular: and #+namespace:");
    if (const auto inc = section(o, node_id{0}, "Includes"); !inc.is_none()) {
        std::string_view code = named_block(o, inc, "includes").value;
        while (!code.empty()) {
            const auto end = code.find('\n');
            const auto line = trim(code.substr(0, end));
            if (line.starts_with("#include"))
                m.includes.emplace_back(trim(line.substr(8)));
            code.remove_prefix(end == std::string_view::npos ? code.size()
                : end + 1);
        }
    }
    for (const auto n : children(o, section(o, node_id{0}, "Messages"))) {
        types::masd_message message;
        message.name = o.titles[n.value];
        for (const auto& [key, value] : properties(o, n)) {
            require_known(key, message_keys, message.name);
            if (key == "response")
                message.response = trim(value);
            else if (key == "http_method")
                message.http_method = trim(value);
            else if (key == "http_path")
                message.http_path = trim(value);
        }
        if (!message.http_method.empty() &&
            std::ranges::find(http_methods, message.http_method) ==
                std::end(http_methods))
            throw model_error(message.name + "'s :http_method: is '" +
                message.http_method + "'; it is one of " +
                listed(http_methods));
        if (message.http_method.empty() != message.http_path.empty())
            throw model_error(message.name + " states one of :http_method: "
                "and :http_path: without the other");
        const bool routed = !message.http_method.empty();
        for (const auto f : children(o, n)) {
            types::masd_field field;
            field.name = o.titles[f.value];
            const auto where = message.name + "." + field.name;
            for (const auto& [key, value] : properties(o, f)) {
                require_known(key, field_keys, where);
                if (key == "cpp_type")
                    field.cpp_type = trim(value);
                else if (key == "default")
                    field.default_value = trim(value);
                else if (key == "values")
                    field.values = words(value);
                else if (key == "encoding")
                    field.encoding = trim(value);
                else if (key == "http_name")
                    field.http_name = trim(value);
                else if (key == "http_in")
                    field.http_in = http_in_of(trim(value), where);
                else if (key == "required") {
                    if (trim(value) != "false")
                        throw model_error(where + "'s :required: is '" +
                            std::string(trim(value)) + "'; it is false, or "
                            "left out");
                    field.required = false;
                }
            }
            if (field.cpp_type.empty())
                throw model_error(where + " states no :cpp_type:");
            if (!field.encoding.empty() && field.encoding != "base64")
                throw model_error(where + "'s :encoding: is '" +
                    field.encoding + "'; it is base64");
            if (routed && field.http_in == types::http_in::none)
                throw model_error(where + " states no :http_in:, which "
                    "every field of a routed request states");
            if (!routed && field.http_in != types::http_in::none)
                throw model_error(where + " states :http_in: on a message "
                    "no route takes");
            if (field.http_in == types::http_in::path && !field.required)
                throw model_error(where + " is a path field, which is "
                    "always given, and states :required: false");
            if (field.http_in == types::http_in::body &&
                message.http_method == "get")
                throw model_error(where + " is a body field of a GET, "
                    "which has no body");
            message.fields.push_back(std::move(field));
        }
        m.messages.push_back(std::move(message));
    }
    return m;
}

void load_masd_operation(load_context& c, types::value_id root) {
    const auto m = read_masd_operation(c.d);
    const auto out = c.b.object(root, "masd_operation");
    c.b.set(out, "component", m.component);
    c.b.set(out, "entity_singular", m.entity);
}

void load_masd_component(load_context& c, types::value_id root) {
    const auto m = read_masd_component(c.d);
    const auto out = c.b.object(root, "masd_component");
    c.b.set(out, "product", m.product);
    c.b.set(out, "component", m.component);
}

void load_masd_entity(load_context& c, types::value_id root) {
    const auto e = read_masd_entity(c.d, c.find);
    const auto m = c.b.object(root, "masd_entity");
    c.b.set(m, "product", e.product);
    c.b.set(m, "component", e.component);
    c.b.set(m, "schema", e.schema);
    c.b.set(m, "entity_singular", e.entity);
    c.b.set(m, "row_type", e.row_type);
}

void load_masd_schema(load_context& c, types::value_id root) {
    const auto s = read_masd_schema(c.d);
    const auto m = c.b.object(root, "masd_schema");
    c.b.set(m, "product", s.product);
    c.b.set(m, "component", s.component);
}

}

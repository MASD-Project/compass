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
#include "compass.crosscheck/ops/generated_shape.hpp"

#include <exception>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include "compass.codegen/ops/entity_steps.hpp"
#include "compass.codegen/ops/model_contexts.hpp"
#include "compass.codegen/ops/physical_space.hpp"
#include "compass.codegen/ops/values.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.model/ops/load.hpp"
#include "compass.model/ops/outline.hpp"

namespace compass::crosscheck::ops {

namespace {

namespace cg = compass::codegen::ops;
namespace co = compass::cpp::ops;
namespace ct = compass::codec::types;
namespace mo = compass::model::ops;
namespace mt = compass::model::types;
namespace values = compass::codegen::ops::values;

using mt::model_tables;
using mt::value_id;

bool is(const model_tables& t, value_id v, std::string_view flag) {
    return values::truthy(t, v, flag);
}

std::string text(const model_tables& t, value_id v, std::string_view key) {
    return std::string(values::str(t, v, key));
}

/**
 * @brief The headings of a model that state what a shape names, as titles
 * that view the document: each child of Columns and of the top-level
 * Indexes by title, the Indexes table of the SQL section, and the Table
 * display of the C++ section.
 */
struct headings {
    std::map<std::string, std::string_view, std::less<>> columns;
    std::map<std::string, std::string_view, std::less<>> indexes;
    std::string_view index_table;
    std::string_view display;

    [[nodiscard]] std::string_view column(std::string_view name) const {
        const auto it = columns.find(name);
        return it == columns.end() ? std::string_view{} : it->second;
    }

    [[nodiscard]] std::string_view index(std::string_view name) const {
        const auto it = indexes.find(name);
        return it == indexes.end() ? index_table : it->second;
    }
};

headings headings_of(const mo::outline& o) {
    const auto title = [&](mo::node_id n) {
        return n.is_none() ? std::string_view{} : o.titles[n.value];
    };
    const mo::node_id document{0};
    headings out;
    for (const auto n : mo::children(o, mo::section(o, document, "Columns")))
        out.columns.emplace(std::string(o.titles[n.value]), o.titles[n.value]);
    for (const auto n : mo::children(o, mo::section(o, document, "Indexes")))
        out.indexes.emplace(std::string(o.titles[n.value]), o.titles[n.value]);
    out.index_table = title(mo::section(o, mo::section(o, document, "SQL"),
        "Indexes"));
    out.display = title(mo::section(o, mo::section(o, document, "C++"),
        "Table display"));
    return out;
}

/**
 * @brief The member type cpp_domain_type_entity.hpp gives a primary-key
 * column, or empty when none of its branches declares one.
 */
std::string key_member(const model_tables& t, value_id c) {
    if (is(t, c, "is_uuid"))
        return "boost::uuids::uuid";
    if (is(t, c, "is_timestamp"))
        return "db_timestamp";
    if (is(t, c, "is_int"))
        return "int";
    if (is(t, c, "is_text"))
        return "std::string";
    return {};
}

/**
 * @brief The member type cpp_domain_type_entity.hpp gives a natural key.
 */
std::string natural_key_member(const model_tables& t, value_id k) {
    if (is(t, k, "is_optional_uuid"))
        return "std::optional<boost::uuids::uuid>";
    if (is(t, k, "is_uuid"))
        return "boost::uuids::uuid";
    if (is(t, k, "is_optional_timestamp"))
        return "std::optional<db_timestamp>";
    if (is(t, k, "is_date"))
        return "std::chrono::year_month_day";
    if (is(t, k, "is_timestamp"))
        return "db_timestamp";
    return text(t, k, "cpp_type");
}

/**
 * @brief The member type cpp_domain_type_entity.hpp gives a plain column of
 * an entity: the first of its branches the column's flags take.
 */
std::string column_member(const model_tables& t, value_id c) {
    const auto cpp = text(t, c, "cpp_type");
    const std::pair<std::string_view, std::string> branches[] = {
        {"is_optional_uuid", "std::optional<boost::uuids::uuid>"},
        {"is_uuid", "boost::uuids::uuid"},
        {"is_optional_timestamp", "std::optional<db_timestamp>"},
        {"is_required_timestamp", "db_timestamp"},
        {"is_optional_date", "std::optional<std::chrono::year_month_day>"},
        {"is_required_date", "std::chrono::year_month_day"},
        {"is_optional_decimal",
            "std::optional<ores::utility::decimal::decimal>"},
        {"is_required_decimal", "ores::utility::decimal::decimal"},
        {"is_required_inet", "std::string"},
        {"is_base64", "std::string"},
        {"is_nullable_string", "std::optional<std::string>"},
        {"is_nullable_numeric", "std::optional<" + cpp + ">"},
        {"is_already_optional", cpp},
        {"is_enum", "std::string"},
        {"is_value_type", "std::string"},
        {"render_is_enum", "std::optional<std::string>"},
        {"is_simple", cpp}};
    for (const auto& [flag, type] : branches) {
        if (is(t, c, flag))
            return type;
    }
    return {};
}

/**
 * @brief The member type cpp_domain_type_entity.hpp gives a side of a
 * junction, and one of its columns.
 */
std::string side_member(const model_tables& t, value_id s) {
    if (is(t, s, "is_uuid"))
        return "boost::uuids::uuid";
    if (is(t, s, "is_date"))
        return "std::chrono::year_month_day";
    return "std::string";
}

std::string junction_column_member(const model_tables& t, value_id c) {
    const std::pair<std::string_view, std::string_view> branches[] = {
        {"is_uuid", "boost::uuids::uuid"},
        {"is_optional_uuid", "std::optional<boost::uuids::uuid>"},
        {"is_optional_timestamp", "std::optional<db_timestamp>"},
        {"is_required_timestamp", "db_timestamp"},
        {"is_date", "std::chrono::year_month_day"},
        {"is_base64", "std::string"}};
    for (const auto& [flag, type] : branches) {
        if (is(t, c, flag))
            return std::string(type);
    }
    return text(t, c, "cpp_type");
}

/**
 * @brief The names of the fields of the field group a model names, or none
 * when it cannot be found or loaded.
 */
using field_reader =
    std::function<std::vector<std::string>(std::string_view group)>;

constexpr std::string_view audit_columns[] = {"modified_by", "performed_by",
    "change_reason_code", "change_commentary"};

/**
 * @brief The shape of a domain entity, after the steps of entity_context
 * its create script, its row, its struct and its table writer read.
 */
generated_shape entity_shape(mo::builder& b, value_id e, const headings& h,
    const field_reader& fields_of) {
    cg::derive_sql_table_flags(b, e);
    cg::suppress_manual_coding_scheme(b, e);
    cg::format_columns(b, e);
    cg::derive_column_flags(b, e);
    cg::derive_field_groups(b, e);
    cg::derive_group_prefixes(b, e);
    cg::derive_natural_key_flags(b, e);
    cg::derive_table_display(b, e);
    cg::derive_storage_shape(b, e);
    cg::derive_sql_names(b, e);
    cg::derive_table_rules(b, e);
    cg::derive_primary_key(b, e);

    const auto& t = b.tables();
    generated_shape s;
    s.name = text(t, e, "entity_singular");
    s.table = text(t, e, "sql_name_base") + "_tbl";
    const bool tenant = is(t, e, "has_tenant_id");
    const bool audit = is(t, e, "has_audit_columns");
    const auto pk = values::items(t, values::member(t, e, "primary_key"),
        "columns");
    const auto keys = values::items(t, e, "natural_keys");
    const auto columns = values::items(t, e, "columns");

    for (const auto c : pk)
        s.table_columns.push_back(text(t, c, "column"));
    if (tenant)
        s.table_columns.emplace_back("tenant_id");
    if (audit)
        s.table_columns.emplace_back("version");
    for (const auto k : keys)
        s.table_columns.push_back(text(t, k, "column"));
    for (const auto c : columns) {
        if (!is(t, c, "is_image_id"))
            s.table_columns.push_back(text(t, c, "name"));
    }
    if (is(t, e, "has_coding_scheme") ||
        is(t, e, "has_nullable_coding_scheme"))
        s.table_columns.emplace_back("coding_scheme_code");
    if (is(t, e, "has_image_id"))
        s.table_columns.emplace_back("image_id");
    if (audit)
        s.table_columns.insert(s.table_columns.end(),
            std::begin(audit_columns), std::end(audit_columns));
    if (!is(t, e, "current_state") && !is(t, e, "no_primary_key")) {
        s.table_columns.emplace_back("valid_from");
        s.table_columns.emplace_back("valid_to");
    }

    if (is(t, e, "has_domain_groups")) {
        for (const auto g : values::items(t, e, "domain_groups"))
            s.members.insert(text(t, g, "member"));
        for (const auto f : values::items(t, e, "domain_group_fields"))
            s.members.insert(text(t, f, "name"));
    } else {
        if (is(t, e, "has_identity_group")) {
            s.members.emplace("identity");
            for (const auto& f : fields_of(text(t, e, "domain_identity_group")))
                s.members.insert("identity." + f);
        } else {
            if (audit)
                s.members.emplace("version");
            if (tenant)
                s.members.emplace("tenant_id");
            for (const auto c : pk)
                s.members.insert(text(t, c, "column"));
            for (const auto k : keys)
                s.members.insert(text(t, k, "column"));
        }
        for (const auto c : columns) {
            if (!is(t, c, "is_identity_group_column") &&
                !is(t, c, "sql_only"))
                s.members.insert(text(t, c, "name"));
        }
        if (is(t, e, "has_audit_group")) {
            s.members.emplace("audit");
            for (const auto& f : fields_of(text(t, e, "domain_audit_group")))
                s.members.insert("audit." + f);
        } else if (audit) {
            s.members.insert(std::begin(audit_columns),
                std::end(audit_columns));
            s.members.emplace("recorded_at");
        }
    }

    const auto cpp = values::member(t, e, "cpp");
    s.iterator = text(t, cpp, "iterator_var");
    for (const auto item : values::items(t, cpp, "table_display"))
        s.display.push_back(text(t, item, "column"));
    s.display_heading = h.display;
    for (const auto index : values::items(t, e, "indexes")) {
        auto name = text(t, index, "name");
        auto at = h.index(name);
        s.indexes.push_back({std::move(name),
            values::py_str(t, values::member(t, index, "columns"), ""), at});
    }

    for (const auto c : pk)
        s.bound.push_back({text(t, c, "column"), key_member(t, c),
            text(t, c, "type"), h.column(text(t, c, "column"))});
    for (const auto k : keys) {
        if (!is(t, k, "is_text_stored"))
            s.bound.push_back({text(t, k, "column"), natural_key_member(t, k),
                text(t, k, "type"), h.column(text(t, k, "column"))});
    }
    for (const auto c : columns) {
        if (!is(t, c, "sql_only") && !is(t, c, "is_text_stored"))
            s.bound.push_back({text(t, c, "name"), column_member(t, c),
                text(t, c, "type"), h.column(text(t, c, "name"))});
    }
    return s;
}

/**
 * @brief The shape of a junction, after the flags junction_context gives
 * its columns and sides.
 */
generated_shape junction_shape(mo::builder& b, value_id j, const headings& h) {
    cg::format_columns(b, j);
    cg::flag_junction_columns(b, j);

    const auto& t = b.tables();
    generated_shape s;
    s.name = text(t, j, "name_singular");
    s.table = values::py_str(t, values::member(t, j, "product"), "None") +
        "_" + values::py_str(t, values::member(t, j, "component"), "None") +
        "_" + values::py_str(t, values::member(t, j, "name"), "None") + "_tbl";
    const auto left = values::member(t, j, "left");
    const auto right = values::member(t, j, "right");
    const bool tenant = is(t, j, "has_tenant_id");
    const auto columns = values::items(t, j, "columns");

    s.table_columns.push_back(text(t, left, "column"));
    if (tenant)
        s.table_columns.emplace_back("tenant_id");
    s.table_columns.push_back(text(t, right, "column"));
    s.table_columns.emplace_back("version");
    for (const auto c : columns)
        s.table_columns.push_back(text(t, c, "name"));
    s.table_columns.insert(s.table_columns.end(), std::begin(audit_columns),
        std::end(audit_columns));
    s.table_columns.emplace_back("valid_from");
    s.table_columns.emplace_back("valid_to");
    if (is(t, j, "history"))
        s.table_columns.emplace_back("closed_by");

    s.members = {"version", text(t, left, "column"), text(t, right, "column"),
        "recorded_at"};
    if (tenant)
        s.members.emplace("tenant_id");
    for (const auto c : columns)
        s.members.insert(text(t, c, "name"));
    s.members.insert(std::begin(audit_columns), std::end(audit_columns));

    const auto cpp = values::member(t, j, "cpp");
    s.iterator = values::truthy(t, cpp, "iterator_var")
        ? text(t, cpp, "iterator_var") : "e";
    for (const auto item : values::items(t, cpp, "table_display"))
        s.display.push_back(text(t, item, "column"));
    s.display_heading = h.display;

    s.bound.push_back({text(t, left, "column"), side_member(t, left),
        text(t, left, "type"), {}});
    s.bound.push_back({text(t, right, "column"), side_member(t, right),
        text(t, right, "type"), {}});
    for (const auto c : columns) {
        const auto name = text(t, c, "name");
        s.bound.push_back({name, junction_column_member(t, c),
            text(t, c, "type"), h.column(name)});
    }
    return s;
}

}

std::optional<generated_shape> generated_shape_of(const ct::corpus_view& v,
    const ct::document& d) {
    if (!d.path.starts_with("projects/") ||
        !co::parent_of(d.path).ends_with("/modeling"))
        return std::nullopt;
    const mo::document doc{d.path, d.source, &d.tables};
    const auto type = mo::model_type_of(mo::declared_type(doc));
    if (type != mt::model_type::entity && type != mt::model_type::junction)
        return std::nullopt;
    const mo::document_finder find = [&v](std::string_view path)
        -> std::optional<mo::document> {
        const auto found = v.find_path(path);
        if (!found)
            return std::nullopt;
        return mo::document{found->path, found->source, &found->tables};
    };
    const mo::document_lister list = [&v](std::string_view prefix) {
        return v.list_files(prefix, ".org");
    };
    mt::model_tables tables;
    mo::builder b(tables);
    mo::load_cache cache;
    std::string error;
    const auto id = mo::load_model(b, doc, *type, find, list, cache, &error);
    if (!error.empty())
        return std::nullopt;
    const auto root = tables.models.roots[id];
    const auto body = values::member(tables, root,
        *type == mt::model_type::entity ? "domain_entity" : "junction");
    if (!values::is(tables, body, mt::value_kind::object))
        return std::nullopt;
    const field_reader fields_of = [&](std::string_view group) {
        std::vector<std::string> out;
        const auto found = find(mo::field_group_path(d.path, group, find));
        if (!found)
            return out;
        mt::model_tables gt;
        mo::builder gb(gt);
        std::string refused;
        const auto g = mo::load_model(gb, *found, mt::model_type::field_group,
            find, list, cache, &refused);
        if (!refused.empty())
            return out;
        const auto fg = values::member(gt, gt.models.roots[g], "field_group");
        for (const auto f : values::items(gt, fg, "fields"))
            out.push_back(text(gt, f, "name"));
        return out;
    };
    mo::outline o;
    mo::build_outline(d.source, d.tables, o);
    const auto h = headings_of(o);
    const bool entity = *type == mt::model_type::entity;
    // A step that refuses the model refuses its generation too, which the
    // codegen codec reports.
    try {
        auto shape = entity ? entity_shape(b, body, h, fields_of)
                            : junction_shape(b, body, h);
        std::vector<cg::drawer_property> properties;
        for (std::size_t i = 0; i < o.file_property_keys.size(); ++i)
            properties.push_back({o.file_property_keys[i],
                o.file_property_values[i]});
        const auto overrides = mo::physical_space_overrides(o, find, cache);
        for (const auto& [key, enabled] : overrides)
            properties.push_back({key, enabled ? "true" : "false"});
        const bool sql_only = values::truthy(tables, body, "sql_only");
        shape.creates_table = cg::archetype_enabled(properties, "ores", entity
            ? "ores.sql.schema.domain_entity_create"
            : "ores.sql.schema.junction_create", "ores.sql.schema", "ores.sql");
        shape.has_row = !sql_only && cg::archetype_enabled(properties,
            "ores", "ores.cpp.repository.entity_header", "ores.cpp.repository",
            "ores.cpp");
        shape.has_table_writer = !sql_only && cg::archetype_enabled(
            properties, "ores", "ores.cpp.domain.table_impl", "ores.cpp.domain",
            "ores.cpp");
        return shape;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}

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
#ifndef COMPASS_CODEGEN_OPS_ENTITY_WIRE_HPP
#define COMPASS_CODEGEN_OPS_ENTITY_WIRE_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.model/ops/builder.hpp"
#include "compass.model/ops/typescript.hpp"
#include "compass.model/types/model_tables.hpp"

/*
 * org_loader's readings of an entity's wire shape, which the protocol,
 * filter and key steps share: the write record, the filter record and the
 * declared key.
 */
namespace compass::codegen::ops::wire {

using compass::model::types::model_tables;
using compass::model::types::value_id;

/**
 * @brief _column_name: column, field or name, the first that is set.
 */
[[nodiscard]] std::string column_name(const model_tables& t, value_id column);

/**
 * @brief write_record_columns: the primary key columns, the natural keys
 * and the columns, the first of each name kept.
 */
[[nodiscard]] std::vector<value_id> write_record_columns(const model_tables& t,
    value_id entity);

/**
 * @brief The names of the primary key columns and the natural keys.
 */
[[nodiscard]] std::vector<std::string> key_column_names(const model_tables& t,
    value_id entity);

/**
 * @brief _column_cpp_type: the cpp_type of the write record column named
 * @p name, or std::string.
 */
[[nodiscard]] std::string column_cpp_type(const model_tables& t,
    value_id entity, std::string_view name);

/**
 * @brief _relation_columns: the filter columns of the extra list requests,
 * then the parent id field when not among them.
 */
[[nodiscard]] std::vector<std::string> relation_columns(const model_tables& t,
    value_id entity);

/**
 * @brief declared_key_field: key_field, else presentation.key_field.
 */
[[nodiscard]] std::string declared_key_field(const model_tables& t,
    value_id entity);

/**
 * @brief declared_key_column: the column named by the declared key among
 * the primary key columns, natural keys and columns; none otherwise.
 */
[[nodiscard]] value_id declared_key_column(const model_tables& t,
    value_id entity);

/**
 * @brief _strip_optional: the type inside std::optional<...>.
 */
[[nodiscard]] std::string_view strip_optional(std::string_view cpp_type);

/**
 * @brief One protocol field, as _ts_field shapes it; a write record field
 * also carries domain_member and render_is_enum.
 */
struct field {
    std::string name;
    std::string cpp_type;
    std::string default_value;
    std::optional<std::string> domain_member;
    bool render_is_enum = false;
};

/**
 * @brief _key_cpp_type: the column's cpp_type, else boost::uuids::uuid
 * when it is_uuid, else std::string.
 */
[[nodiscard]] std::string key_cpp_type(const model_tables& t,
    value_id column);

/**
 * @brief key_record_fields: the declared key with its column's type, else
 * the primary key columns.
 */
[[nodiscard]] std::vector<field> key_record_fields(const model_tables& t,
    value_id entity);

/**
 * @brief Whether @p name is one of SERVICE_PRESERVED_FIELDS, the
 * server-owned fields an update carries forward from the row it edits.
 */
[[nodiscard]] bool is_service_preserved(std::string_view name);

/**
 * @brief write_record_for: the fields a client may send.
 */
[[nodiscard]] std::vector<field> write_record_for(const model_tables& t,
    value_id entity);

/**
 * @brief One member of an entity's filter record (filter_members).
 */
struct filter_member {
    enum class kind { equals, one_of, range };
    kind k = kind::equals;
    std::string member;
    std::string column;
    std::string cpp_type;
    std::string read_type{};
    bool is_nullable = false;
    bool is_key = false;
    bool is_from = false;
    bool is_timestamp = false;
};

/**
 * @brief filter_members: the record's members and the searchable columns.
 */
struct filter_spec {
    std::vector<filter_member> members;
    std::vector<std::string> searchable;
};
[[nodiscard]] filter_spec filter_members(const model_tables& t,
    value_id entity);

/**
 * @brief Writes @p f as an object appended to @p list: name, cpp_type, an
 * empty comment, default when set, ts_type when _ts_type maps the type,
 * and the write record members when set.
 */
void append_field(compass::model::ops::builder& b, value_id list,
    const field& f, const compass::model::ops::custom_types& custom);

}

#endif

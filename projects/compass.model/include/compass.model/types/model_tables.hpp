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
#ifndef COMPASS_MODEL_TYPES_MODEL_TABLES_HPP
#define COMPASS_MODEL_TYPES_MODEL_TABLES_HPP

#include <cstdint>
#include <string_view>
#include <vector>
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"

namespace compass::model::types {

using compass::org::types::column;

using model_id = compass::org::types::handle<struct model_tag>;
using value_id = compass::org::types::handle<struct value_tag>;
using text_id = compass::org::types::handle<struct text_tag>;

/**
 * @brief The codegen model types, named after their #+type: without the
 * ores.codegen. prefix.
 */
enum class model_type : std::uint8_t {
    entity,
    component,
    operation,
    field_group,
    junction,
    family,
    service_registry,
    asset_class_catalogue,
    trade_type_catalogue,
    outcome_catalogue,
    masd_entity,
    masd_schema,
    masd_component,
    masd_operation
};

inline constexpr model_type model_types[] = {
    model_type::entity, model_type::component, model_type::operation,
    model_type::field_group, model_type::junction, model_type::family,
    model_type::service_registry, model_type::asset_class_catalogue,
    model_type::trade_type_catalogue, model_type::outcome_catalogue,
    model_type::masd_entity, model_type::masd_schema,
    model_type::masd_component, model_type::masd_operation};

/**
 * @brief The name of @p t as #+type: writes it after ores.codegen, or, for
 * a type of the MASD library, after masd.codegen with masd_ before it.
 */
[[nodiscard]] constexpr std::string_view name_of(model_type t) noexcept {
    switch (t) {
    case model_type::entity: return "entity";
    case model_type::component: return "component";
    case model_type::operation: return "operation";
    case model_type::field_group: return "field_group";
    case model_type::junction: return "junction";
    case model_type::family: return "family";
    case model_type::service_registry: return "service_registry";
    case model_type::asset_class_catalogue: return "asset_class_catalogue";
    case model_type::trade_type_catalogue: return "trade_type_catalogue";
    case model_type::outcome_catalogue: return "outcome_catalogue";
    case model_type::masd_entity: return "masd_entity";
    case model_type::masd_schema: return "masd_schema";
    case model_type::masd_component: return "masd_component";
    case model_type::masd_operation: return "masd_operation";
    }
    return "";
}

/**
 * @brief What a value is. A boolean, integer or string holds its payload in
 * scalars; a list or object holds its children.
 */
enum class value_kind : std::uint8_t {
    null,
    boolean,
    integer,
    string,
    list,
    object
};

/**
 * @brief The values of every model: one row per object, list or scalar.
 *
 * The children of a list or object are a chain: firsts and lasts give its
 * first and last child, nexts the following sibling. A child of an object
 * has a key; any other value's key is none. scalars holds 0 or 1 for a
 * boolean, the number for an integer and the text_id for a string. A value
 * that is replaced or removed stays as a row nothing points to.
 */
struct value_table {
    column<value_id, value_kind> kinds;
    column<value_id, text_id> keys;
    column<value_id, std::int64_t> scalars;
    column<value_id, value_id> firsts;
    column<value_id, value_id> lasts;
    column<value_id, value_id> nexts;

    [[nodiscard]] std::size_t size() const noexcept { return kinds.size(); }
};

/**
 * @brief Distinct strings, keys and values alike, end to end in bytes.
 */
struct text_table {
    column<text_id, std::uint32_t> firsts;
    std::vector<char> bytes;

    [[nodiscard]] std::size_t size() const noexcept { return firsts.size(); }
};

/**
 * @brief One row per loaded model: where it was read, its type and the
 * object ores.codegen's loader returns for it.
 */
struct model_table {
    column<model_id, text_id> paths;
    column<model_id, model_type> types;
    column<model_id, value_id> roots;

    [[nodiscard]] std::size_t size() const noexcept { return paths.size(); }
};

/**
 * @brief The loaded models of a checkout.
 */
struct model_tables {
    model_table models;
    value_table values;
    text_table texts;
};

/**
 * @brief The text of @p t.
 */
[[nodiscard]] inline std::string_view text_of(const text_table& texts,
    text_id t) noexcept {
    const auto first = texts.firsts[t];
    const auto end = t.next().value < texts.size()
        ? texts.firsts[t.next()]
        : static_cast<std::uint32_t>(texts.bytes.size());
    return {texts.bytes.data() + first, end - first};
}

}

#endif

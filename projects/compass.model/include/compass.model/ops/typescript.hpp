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
#ifndef COMPASS_MODEL_OPS_TYPESCRIPT_HPP
#define COMPASS_MODEL_OPS_TYPESCRIPT_HPP

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.model/ops/load.hpp"

/**
 * @brief The TypeScript projections of C++ member types that org_loader
 * computes for the loaders and that generate_from_model reuses.
 */
namespace compass::model::ops {

/**
 * @brief The (type, header) rows of the custom-type registry, sorted by
 * type.
 */
using custom_types = std::vector<std::pair<std::string, std::string>>;

/**
 * @brief The rows of projects/modeling/cpp_custom_types.org, as
 * _load_custom_type_headers parses them; none when the checkout lacks the
 * registry. Cached in @p cache while the document is unchanged.
 */
[[nodiscard]] const custom_types& custom_type_headers(
    const document_finder& find, load_cache& cache);

/**
 * @brief The (type, header) rows of the same registry's Text form column:
 * the header that states each type's text_form for the JSON codec, for the
 * types that have one; none when the registry lacks the column.
 */
[[nodiscard]] const custom_types& custom_type_text_forms(
    const document_finder& find, load_cache& cache);

/**
 * @brief _ts_type: the TypeScript type of @p cpp_type, or empty when it has
 * no projection.
 */
[[nodiscard]] std::string ts_type(std::string_view cpp_type,
    const custom_types& custom);

/**
 * @brief _ts_domain_type: _ts_type for a domain struct member, which also
 * projects a uuid, a date and an address to string and an optional to a
 * nullable type.
 */
[[nodiscard]] std::string ts_domain_type(std::string_view cpp_type,
    const custom_types& custom);

/**
 * @brief One import of field_group_ts_imports: an interface and the module
 * that declares it.
 */
struct ts_import {
    std::string pascal;
    std::string module;
};

/**
 * @brief field_group_ts_imports: the modules the interface of a field
 * group (or entity) of @p component named @p own imports for its members'
 * @p cpp_types, sorted by interface name.
 */
[[nodiscard]] std::vector<ts_import> field_group_ts_imports(
    std::string_view component, std::string_view own,
    const std::vector<std::string>& cpp_types, const custom_types& custom);

/**
 * @brief One import of ts_domain_imports: a domain entity, its interface
 * and the directory of its module relative to the protocol module.
 */
struct ts_domain_import {
    std::string entity;
    std::string entity_pascal;
    std::string module_dir;
};

/**
 * @brief ts_domain_imports: the domain interfaces that @p cpp_types name
 * as ores::<owner>::domain::<entity>, by entity, a registered custom type
 * and a hand-written utility interface left out. The module is beside
 * the protocol's when @p component is none or owns it.
 */
[[nodiscard]] std::vector<ts_domain_import> ts_domain_imports(
    const std::vector<std::string_view>& cpp_types,
    std::optional<std::string_view> component, const custom_types& custom);

/**
 * @brief ts_utility_imports: the hand-written interfaces of
 * _TS_UTILITY_DOMAIN_TYPES that @p cpp_types name, a registered name
 * counting only when no identifier character follows it, each with its
 * module, sorted by interface name.
 */
[[nodiscard]] std::vector<ts_import> ts_utility_imports(
    const std::vector<std::string_view>& cpp_types);

/**
 * @brief _to_pascal_case.
 */
[[nodiscard]] std::string pascal_case(std::string_view name);

}

#endif

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
#ifndef COMPASS_CODEGEN_OPS_ENTITY_SURFACES_HPP
#define COMPASS_CODEGEN_OPS_ENTITY_SURFACES_HPP

#include <string>
#include <string_view>

/**
 * @brief The sentinels a generated script or recipe sends for a value it
 * has no row for, shared by the shell plan and the recipes.
 */
namespace compass::codegen::ops::surfaces {

inline constexpr std::string_view sentinel_reason = "system.new_record";
inline constexpr std::string_view sentinel_commentary = "generated_script";

/**
 * @brief _sentinel_value: a well-formed value of @p cpp_type that
 * addresses no row; the absent token for an optional.
 */
[[nodiscard]] std::string sentinel_value(std::string_view cpp_type);

/**
 * @brief _sentinel_for_field: the reason or commentary sentinel by name,
 * else _sentinel_value.
 */
[[nodiscard]] std::string sentinel_for_field(std::string_view name,
    std::string_view cpp_type);

}

#endif

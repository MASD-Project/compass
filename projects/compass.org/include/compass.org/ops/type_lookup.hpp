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
#ifndef COMPASS_ORG_OPS_TYPE_LOOKUP_HPP
#define COMPASS_ORG_OPS_TYPE_LOOKUP_HPP

#include <string_view>
#include "compass.org/types/type_registry.hpp"

namespace compass::org::ops {

/**
 * @brief The text of an interned string of the registry.
 */
[[nodiscard]] std::string_view name_text(const types::type_registry& r,
    types::name_id n) noexcept;

/**
 * @brief The type whose name is @p name, by binary search; none when no
 * type has that name. The match is exact and case-sensitive.
 */
[[nodiscard]] types::type_id find_type(const types::type_registry& r,
    std::string_view name) noexcept;

/**
 * @brief The active TODO keywords of @p t, in contract order.
 */
[[nodiscard]] types::column_view<types::type_entry_id, types::name_id>
active_todos(const types::type_registry& r, types::type_id t) noexcept;

/**
 * @brief The done TODO keywords of @p t, in contract order.
 */
[[nodiscard]] types::column_view<types::type_entry_id, types::name_id>
done_todos(const types::type_registry& r, types::type_id t) noexcept;

/**
 * @brief The titles of the sections @p t requires, in contract order.
 */
[[nodiscard]] types::column_view<types::type_entry_id, types::name_id>
required_sections(const types::type_registry& r, types::type_id t) noexcept;

/**
 * @brief True when @p t has a TODO vocabulary.
 */
[[nodiscard]] bool is_stateful(const types::type_registry& r,
    types::type_id t) noexcept;

}

#endif

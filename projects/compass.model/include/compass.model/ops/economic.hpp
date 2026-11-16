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
#ifndef COMPASS_MODEL_OPS_ECONOMIC_HPP
#define COMPASS_MODEL_OPS_ECONOMIC_HPP

#include <string>
#include <string_view>
#include <vector>
#include "compass.model/ops/builder.hpp"

/**
 * @brief org_loader's economic declaration of a struct, which the field
 * group loader and generate_from_model's entity enrichment both read.
 */
namespace compass::model::ops {

/**
 * @brief _member_name: the name or column of a member, or empty.
 */
[[nodiscard]] std::string_view member_name(const builder& b,
    types::value_id member);

/**
 * @brief What economic_declaration returns: whether the members declare
 * :economic:, and the names of those that are.
 */
struct economic {
    bool declared = false;
    std::vector<std::string> members;
};

/**
 * @brief economic_declaration: whether @p members, a struct's flat
 * members, declare :economic:, and which do. Throws model_error when a
 * column of @p carried, which reaches no flat member, declares it, when a
 * declaration is not a boolean, or when only some members declare it.
 */
[[nodiscard]] economic economic_declaration(const builder& b,
    std::string_view owner, const std::vector<types::value_id>& members,
    const std::vector<types::value_id>& carried);

}

#endif

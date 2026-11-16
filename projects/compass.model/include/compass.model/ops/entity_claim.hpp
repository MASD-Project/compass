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
#ifndef COMPASS_MODEL_OPS_ENTITY_CLAIM_HPP
#define COMPASS_MODEL_OPS_ENTITY_CLAIM_HPP

#include <optional>
#include <string>
#include <string_view>

namespace compass::model::ops {

/**
 * @brief The table an org file's raw text claims for an entity, as
 * _entity_org_by_table scans it: the first #+entity_singular: at the start
 * of a line, the first ":tablename: <name>_tbl" and the first
 * #+entity_plural:, or the singular with an s when there is none.
 */
struct entity_claim {
    std::string table;
    std::string entity_singular;
    std::string entity_plural;
};

/**
 * @brief The claim of @p org_text, or none when it states no entity
 * singular or no table name.
 */
[[nodiscard]] std::optional<entity_claim> entity_claim_of(
    std::string_view org_text);

}

#endif

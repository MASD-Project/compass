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
#ifndef COMPASS_CODEGEN_OPS_FACTS_HPP
#define COMPASS_CODEGEN_OPS_FACTS_HPP

#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.codegen/types/entity_registry.hpp"
#include "compass.codegen/types/physical_space.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codegen::ops {

/**
 * @brief A fact: its name and the hash of its value.
 */
using fact = std::pair<std::string, compass::store::types::blob_id>;

/**
 * @brief The fact named @p name whose value is @p bytes.
 */
[[nodiscard]] fact fact_of(std::string name, std::string_view bytes);

/**
 * @brief Adds to @p out the facts of every collection of @p r: one for
 * the whole collection, named as the collection is, and, for a map or a
 * set, one per element, named types::fact_name(collection, key). The
 * value of a fact is every field of what it names, so a model that read
 * it is generated again exactly when one of them changes.
 */
void publish_facts(std::vector<fact>& out, const types::entity_registry& r);
void publish_facts(std::vector<fact>& out,
    const types::checkout_registry& r);

}

#endif

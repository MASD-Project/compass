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
#ifndef COMPASS_MODEL_OPS_MASD_ENTITY_HPP
#define COMPASS_MODEL_OPS_MASD_ENTITY_HPP

#include "compass.model/ops/load.hpp"
#include "compass.model/types/masd_entity.hpp"

namespace compass::model::ops {

/**
 * @brief Reads @p d, a model of #+type: masd.codegen.entity, and the
 * profile page its * Flags drawer binds, which @p find gives.
 * @throws model_error naming what the model says that the format does not
 * have: an unknown property, profile, profile feature, section or table
 * header, a value outside a property's list, or a column that a foreign
 * key, index or unique constraint names and the model lacks.
 */
[[nodiscard]] types::masd_entity read_masd_entity(const document& d,
    const document_finder& find);

/**
 * @brief Reads @p d, a model of #+type: masd.codegen.schema.
 * @throws model_error as read_masd_entity does.
 */
[[nodiscard]] types::masd_schema read_masd_schema(const document& d);

/**
 * @brief Reads @p d, a model of #+type: masd.codegen.component.
 * @throws model_error when its * Flags drawer states a property other than
 * product and component, or leaves one of them out.
 */
[[nodiscard]] types::masd_component read_masd_component(const document& d);

}

#endif

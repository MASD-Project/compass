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
#ifndef COMPASS_CODEGEN_OPS_MASD_CONTEXT_HPP
#define COMPASS_CODEGEN_OPS_MASD_CONTEXT_HPP

#include <span>
#include "compass.model/ops/builder.hpp"
#include "compass.model/ops/typescript.hpp"
#include "compass.model/types/masd_entity.hpp"

namespace compass::codegen::ops {

/**
 * @brief The context the masd.cpp templates render a table's row type and
 * COPY writer from: the library values and, under masd_entity, the names,
 * includes, enumerations, members and COPY fields @p e implies. @p custom
 * gives the header of each C++ type the standard library does not have.
 * @throws std::runtime_error when a column of the row has a SQL type with
 * no C++ type and the model states none.
 */
[[nodiscard]] compass::model::types::value_id masd_entity_context(
    compass::model::ops::builder& b, compass::model::types::value_id library,
    const compass::model::types::masd_entity& e,
    const compass::model::ops::custom_types& custom);

/**
 * @brief The context the masd.sql template renders a component's whole
 * schema from: the library values and, under masd_schema, each schema of
 * @p s with the tables of @p tables in it, in an order where every table
 * follows the tables it references, ties broken by name, then the
 * functions. A table of a schema @p s does not list is left out, as the
 * bookkeeping schema the migration runner creates is.
 * @throws std::runtime_error when a foreign key references a table
 * schema.sql does not create, or the references form a cycle.
 */
[[nodiscard]] compass::model::types::value_id masd_schema_context(
    compass::model::ops::builder& b, compass::model::types::value_id library,
    const compass::model::types::masd_schema& s,
    std::span<const compass::model::types::masd_entity> tables);

}

#endif

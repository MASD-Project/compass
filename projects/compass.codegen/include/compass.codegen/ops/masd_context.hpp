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
#include <string_view>
#include "compass.model/ops/builder.hpp"
#include "compass.model/ops/typescript.hpp"
#include "compass.model/types/masd_entity.hpp"

namespace compass::codegen::ops {

/**
 * @brief The context the masd.cpp templates render a table's row type and
 * COPY writer and JSON codec from: the library values and, under
 * masd_entity, the names, includes, enumerations, members, COPY fields and
 * wire fields @p e implies. @p custom gives the header of each C++ type the
 * standard library does not have, and @p text_forms the header that states
 * the text form of each such type the codec writes.
 * @throws std::runtime_error when a column of the row has a SQL type with
 * no C++ type and the model states none.
 */
[[nodiscard]] compass::model::types::value_id masd_entity_context(
    compass::model::ops::builder& b, compass::model::types::value_id library,
    const compass::model::types::masd_entity& e,
    const compass::model::ops::custom_types& custom,
    const compass::model::ops::custom_types& text_forms);

/**
 * @brief The context the masd.cpp.codec templates render a component's
 * JSON codec runtime from: the library values and, under masd_component,
 * the runtime's namespace, include directory and include guard prefix,
 * each from @p component_dir, the component's directory under projects/,
 * such as compass.db.
 */
[[nodiscard]] compass::model::types::value_id masd_component_context(
    compass::model::ops::builder& b, compass::model::types::value_id library,
    std::string_view component_dir);

/**
 * @brief The context the masd.cpp.protocol and masd.cpp.json templates
 * render an operation model's messages and their JSON codecs from: the
 * library values and, under masd_operation, the namespace, the includes,
 * each enumeration a field's :values: declares, and each message after the
 * messages it holds, with its fields. A field encoded as base64 holds the
 * codec runtime's base64_bytes. @p codec_component names the component of
 * @p product whose masd.cpp.codec runtime the codecs are written against.
 * @throws std::runtime_error when two fields list different values for one
 * enumeration, or the messages hold each other in a cycle.
 */
[[nodiscard]] compass::model::types::value_id masd_operation_context(
    compass::model::ops::builder& b, compass::model::types::value_id library,
    const compass::model::types::masd_operation& op,
    std::string_view product, std::string_view codec_component);

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

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
#include "compass.codegen/ops/entity_context.hpp"

#include <string>
#include "compass.codegen/ops/entity_steps.hpp"
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

using compass::model::types::value_id;
using compass::model::types::value_kind;

value_id entity_context(compass::model::ops::builder& b, value_id library,
    value_id model, std::string_view model_key) {
    const auto context = b.detached_object();
    for (const auto c : values::items(b.tables(), library))
        b.set_copy(context, std::string(values::key_of(b.tables(), c)), c);
    b.set_copy(context, model_key, model);
    const auto entity = values::member(b.tables(),
        b.find(context, model_key), "domain_entity");
    if (!values::is(b.tables(), entity, value_kind::object))
        return context;
    const auto loaded = values::member(b.tables(), model, "domain_entity");
    derive_sql_table_flags(b, entity);
    suppress_manual_coding_scheme(b, entity);
    mark_artefact_last_items(b, entity);
    derive_cardinality_limit_flag(b, entity);
    format_columns(b, entity);
    derive_column_flags(b, entity);
    derive_column_aggregates(b, entity);
    project_sql_table(b, context, loaded, entity);
    b.set_copy(context, "domain_entity", entity);
    return context;
}

}

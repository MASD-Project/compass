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
#ifndef COMPASS_CODEGEN_OPS_ENTITY_STEPS_HPP
#define COMPASS_CODEGEN_OPS_ENTITY_STEPS_HPP

#include <string_view>
#include "compass.model/ops/builder.hpp"
#include "compass.model/types/model_tables.hpp"

/**
 * @brief The enrichment steps of ores.codegen's generate_from_model for a
 * domain entity (core.py, the domain-entity block from line 4113), one
 * function per step, in the order entity_context runs them. Each takes the
 * entity, the domain_entity object of the model under its key, and changes
 * it in place as the Python changes its dict.
 *
 * Python aliasing. The Python builds data['table'] first, as a projection
 * of the entity that shares some of its objects (insert_trigger,
 * check_constraints, indexes, the primary key's columns, and validation_fn
 * once synced back); the oracle dumps the final state of those shared
 * objects. Here the projection is project_sql_table, run after every other
 * step: it reads its snapshot parts from the entity as loaded and copies
 * its shared parts from the enriched entity. derive_sql_table_flags makes
 * on the entity the changes the projection's normalisation makes on the
 * shared objects, at the point the Python makes them. A step that replaces
 * one of the shared objects wholesale, rather than changing it in place,
 * breaks the sharing in the Python and must be reflected in
 * project_sql_table.
 */
namespace compass::codegen::ops {

/**
 * @brief _mark_last_item: sets @p key to true on the last item of the list
 * @p list when that item is an object; nothing otherwise.
 */
void mark_last_item(compass::model::ops::builder& b,
    compass::model::types::value_id list, std::string_view key = "last");

/**
 * @brief The changes the SQL table projection makes to the entity
 * (core.py 4113-4137 and normalise_sql_table_context): the last markers on
 * the lists the table shares (indexes, check_constraints,
 * insert_trigger.validations), the tenant-scope flags and the order_by
 * default on validation_fn, and the coding-scheme and image flags synced
 * back from the table.
 */
void derive_sql_table_flags(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief Clears has_coding_scheme and has_nullable_coding_scheme when the
 * entity declares its own coding_scheme_code column (core.py 4138-4152),
 * so the generic column is not emitted twice; has_any_coding_scheme stays.
 */
void suppress_manual_coding_scheme(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief Marks the last item of artefact_indexes and artefact_columns
 * (core.py 4153-4156).
 */
void mark_artefact_last_items(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief sql.has_cardinality_limit_validations, true when an insert
 * trigger validation names a cardinality_limit_table (core.py 4157-4161);
 * absent otherwise.
 */
void derive_cardinality_limit_flag(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief Marks the last column and formats each column's detail for a
 * doxygen comment, prefixing continuation lines with "     * "
 * (core.py 4164-4166, _format_columns_for_doxygen).
 */
void format_columns(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The per-column flags of core.py 4167-4557, on every column, natural
 * key and primary-key column: the type and representation flags each layer
 * switches on (is_uuid, is_optional_timestamp, is_date, is_decimal,
 * is_simple, is_text_stored and the rest), the render_* flags of the
 * domain member's type (a field group's declaration for a domain-grouped
 * column), render_label, the bool and int default_value, iter_var, the
 * jsonb generator_expr, sql_only and, unless sql_only, no_wire. The date
 * and decimal projections apply to plain columns only.
 */
void derive_column_flags(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The has_* column aggregates and wire_left_out of core.py
 * 4558-4605, read from the columns as derive_column_flags leaves them.
 */
void derive_column_aggregates(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief data['table']: domain_entity_to_table_context and
 * normalise_sql_table_context, written to table of @p context. The
 * snapshot parts (the scalars, the primary key's own members, the column
 * projections, the description comment and the derived flags) come from
 * @p loaded, the entity as the model holds it; the parts the Python shares
 * with the entity come from @p entity, enriched. Runs after every other
 * step.
 */
void project_sql_table(compass::model::ops::builder& b,
    compass::model::types::value_id context,
    compass::model::types::value_id loaded,
    compass::model::types::value_id entity);

}

#endif

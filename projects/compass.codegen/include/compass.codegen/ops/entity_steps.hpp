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
#include "compass.codegen/types/entity_registry.hpp"
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
 * @brief The field-group contract of core.py 4606-4654: has_identity_group,
 * has_audit_group, identity_group_type, audit_group_qualified, each domain
 * group's type_qualified (unqualified when the group is the entity's own
 * component's) and header, and has_domain_groups.
 */
void derive_field_groups(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief Each column's is_identity_group_column and group_prefix, the
 * member it is reached through, and the entity's scaffold_prefix and
 * audit_prefix (core.py 4655-4682). Reads has_identity_group and
 * has_audit_group from derive_field_groups.
 */
void derive_group_prefixes(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief stamp_members and has_stamp_members (core.py 4683-4710): the
 * domain groups holding a field stamp() writes or, without domain groups,
 * the scaffold and audit members.
 */
void derive_stamp_members(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The group prefixes of the primary key, its columns and the
 * natural keys (core.py 4711-4719).
 */
void derive_key_group_prefixes(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief Who supplies each field (core.py 4720-4776): is_user_supplied,
 * is_minted, is_session_party and member_access on the columns, the
 * primary key and its members (a uuid key is minted by default), then
 * user_supplied_count, is_bool and enum_short_name, has_bool_columns and
 * has_session_party_columns. An entity without a primary key counts the
 * empty key Python's .get('primary_key', {}) gives it as one user-supplied
 * member.
 */
void derive_supply(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The shell tests' tokens (core.py 4777-4866): positional_index and
 * sample_token on each user-supplied key member and column, in read
 * order, malformed_token_column (the first whose parse can reject a token,
 * or null) and add_positional_count.
 */
void derive_sample_tokens(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief Puts the identity, audit and domain group headers, sorted, before
 * the model's own cpp.includes.domain (core.py 4867-4888).
 */
void inject_group_includes(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The natural-keys block of core.py 4889-4939: the artefact last
 * marker (false when plain columns follow), each key's iter_var, is_uuid,
 * is_int, is_timestamp, is_date, optional flags and render_label, then
 * has_multiple_natural_keys, has_text_natural_keys, has_date_natural_keys,
 * has_date_or_timestamp_natural_keys and the composite key names. Without
 * natural keys, has_text_natural_keys defaults to false.
 */
void derive_natural_key_flags(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief unique_keys, has_date_or_timestamp_natural_keys broadened to
 * timestamp columns, has_unique_suffix_columns, needs_counter and
 * has_uuid_to_string_generator (core.py 4940-4997).
 */
void derive_key_aggregates(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief Marks the last item of indexes and validations (core.py
 * 4998-5001).
 */
void mark_index_and_validation_last_items(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief description_formatted, description_lines and description_oneline
 * (core.py 5002-5014, _format_description_as_comment).
 */
void format_description(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The component's include, directory, core and service names and
 * their upper-case forms, component_is_database and the cache component
 * cached_by names (core.py 5015-5061).
 */
void derive_component_names(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The entity's upper, pascal, short, plural and title name forms
 * (core.py 5062-5091).
 */
void derive_entity_names(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The table display rows' iter_var, member-prefixed column,
 * is_uuid, is_optional and is_bool, and cpp.has_uuid_table_display and
 * cpp.has_optional_table_display (core.py 5092-5129,
 * _prepare_table_display).
 */
void derive_table_display(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief Copies each repository member to the entity's top level (core.py
 * 5130-5133). The Python assigns the same object, so a later change to a
 * copied list or object shows under repository too; a later step that
 * makes one must make it in both places.
 */
void copy_repository_fields(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief read_tenant_filtered (derive_tenant_read_flags),
 * system_tenant_visible, tenant_where and sys_decl (core.py 5134-5170).
 */
void derive_tenant_scope(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The messaging handler defaults a model may override: the delete
 * and history request id fields, the composite key's extra arguments and
 * single_delete (core.py 5171-5191).
 */
void derive_messaging_defaults(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief extra_list_requests from the foreign keys opted in with list_by,
 * and each such key's list_by_order_column and list_by_order_desc (core.py
 * 5194-5228).
 */
void derive_list_requests(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief apply_stated_order (core.py 5229): sortable_fields,
 * order_key_columns, default_order_columns, default_order_desc and each
 * list_by foreign key's default order.
 */
void apply_stated_order(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The foreign keys' targets and the eventing test's parent seeding
 * (core.py 5245-5422): target_immutable, target_no_validity, and for an
 * enforced key matches_tenant and skip_check; each key's group_prefix;
 * for a mandatory key whose parent @p registry resolves, the parent_*
 * fields and parent_required_fks, the ancestors to seed first, closed
 * over the foreign-key chain (_plan_required_seeds); then
 * has_system_tenant_fks, seed_party, seed_parent_country_sentinel and
 * seed_parent_currency. Nothing without foreign keys.
 */
void derive_foreign_key_seeds(compass::model::ops::builder& b,
    compass::model::types::value_id entity,
    const types::entity_registry& registry);

/**
 * @brief index_name_prefix (sql.index_prefix, else entity_plural),
 * has_party_column, party_column_optional and party_scoped_writes (core.py
 * 5423-5465). Reads parent_is_party from derive_foreign_key_seeds.
 */
void derive_party_scope(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The table's shape (core.py 5466-5559): current_state,
 * has_as_of_lookup (on unless the model says otherwise), immutable,
 * append_insert, nullable_tenant_id, has_newest_read, fixed_columns,
 * immutable_tenant_key, has_tenant_in_pk, validates_tenant_on_insert, and
 * on the sql section hypertable, bitemporal_soft_update and rls_hot_path.
 */
void derive_storage_shape(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief sql_name_base, the base every SQL identifier is composed from,
 * cut so its longest suffix fits PostgreSQL's 63 bytes; each enforced
 * foreign key's constraint_name unless the model states one; and
 * rls_table_base, the base without its <product>_<component>_ prefix
 * (core.py 5560-5606 and 5609-5631).
 */
void derive_sql_names(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief _resolve_pinned_keys (core.py 5607, 2684-2768): each pinned key's
 * matches_tenant, enforced (its target is immutable, as @p registry says),
 * column_pairs, first_column, column_list, target_column_list and
 * constraint_name, then enforced_pinned_keys and checked_pinned_keys.
 * Reads sql_name_base.
 */
void resolve_pinned_keys(compass::model::ops::builder& b,
    compass::model::types::value_id entity,
    const types::entity_registry& registry);

/**
 * @brief _resolve_routed_check (core.py 5608, 2648-2683): for a model
 * routed by a trade type catalogue, a check that the routed column holds
 * one of the codes the catalogue routes to this entity, put first in
 * sql.extra_checks.
 */
void resolve_routed_check(compass::model::ops::builder& b,
    compass::model::types::value_id entity,
    const types::entity_registry& registry);

/**
 * @brief enforced_foreign_keys (core.py 5306), the foreign keys with
 * enforce set. The Python's list holds the very key objects, so the
 * oracle shows them as later steps leave them: run after the last change
 * to a foreign key.
 */
void project_enforced_foreign_keys(compass::model::ops::builder& b,
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

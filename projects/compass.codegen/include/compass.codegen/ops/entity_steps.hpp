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
#include <vector>
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
 * @brief The table's rules of core.py 5632-5736: has_gist_exclusion,
 * has_audit_columns, change_reason_code_declared and each insert trigger
 * validation's nullable, taken from its column; for a current-state table
 * no current-only index, no as_of read, touch function or bi-temporal
 * trigger; the last markers of the foreign keys and the sql section's
 * fk_copy_validations, text_code_validations and extra_delete_sets; and
 * the temporal_filter, list_temporal_filter and version_filter fragments.
 */
void derive_table_rules(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The primary key's enrichment (core.py 5738-6058): the type flags,
 * uuid_check_fn, default cpp_type and render_label of the key and each of
 * its columns (_enrich_primary_key_field), the columns' last marker and
 * key_text, removal_key_text and row_text, has_timestamp_key_columns
 * (which also sets has_date_or_timestamp_natural_keys), and the strings
 * the SQL, repository, batch, eventing and notify templates splice: the
 * storage key widened by :sql_key: columns, the parameter and argument
 * lists, key_columns (copies taken before value_text is set), the log
 * fields and the notify key, which is the declared key when the model
 * states one. Nothing without a primary key.
 */
void derive_primary_key(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The presentation's list columns (core.py 6059-6177): the default
 * domain_class, the last markers on the columns and on the last of the
 * columns hidden by default, the optional-type flags from the domain
 * column types, the column_index and column_style defaults, and the
 * has_badge_columns and has_self_colour_columns flags. Nothing without a
 * presentation; only domain_class without its columns.
 */
void derive_presentation_columns(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The presentation's controller knobs and default detail form
 * (core.py 6178-6266): has_parent_relationship, the item_var default and,
 * when the model states no detail_fields, the code, name and description
 * rows the entity's members allow. Nothing without a presentation.
 */
void derive_presentation_detail_defaults(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The per-field flags of presentation.detail_fields (core.py
 * 6267-6403): widget kinds, combo promotions and helpers, the type flags
 * from the domain, natural key and primary key types, the position
 * markers, the key and lock flags and the derived widget names. Nothing
 * without a presentation.
 */
void derive_detail_fields(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The presentation's list and dialog flags (core.py 6404-6451):
 * the has_pagination, has_readonly_paginated_list and
 * has_parent_scoped_list defaults, has_combo_badge_source and the
 * delete_request_id_field default. Nothing without a presentation.
 */
void derive_presentation_flags(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief generator_facet_name, defaulted to generators, and its upper
 * case form (core.py 6452-6453).
 */
void derive_generator_facet_names(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief has_uuid_columns and has_render_uuid_columns (core.py
 * 6454-6483): a uuid column (by the repository flags, or by the render
 * flags) or natural key, else the primary key's is_uuid as it is.
 */
void derive_uuid_column_flags(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The history field mapper's datetime and string gates (core.py
 * 6484-6516), has_render_datetime_columns and has_render_string_columns,
 * from the wire columns' render kinds and the natural keys.
 */
void derive_render_kind_flags(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief _mapper_include_gates (core.py 2931, applied at 6517): the
 * mapper's enum domain headers, sorted, and its optional, string, datetime
 * and string_view gates.
 */
void derive_mapper_include_gates(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief repository_uses_uuid_io and has_uuid_include (core.py
 * 6518-6534). has_uuid_include is Python's `or` chain: the first true
 * operand of has_parent_id, service_find_by_uuid and
 * service_find_by_code.parent_column as it is, else whether a foreign key
 * lists by uuid.
 */
void derive_uuid_include_gates(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The TypeScript member types (core.py 6571-6579): ts_type, as
 * _ts_domain_type projects its cpp_type, on each column, natural key,
 * primary key column and the primary key itself, when one projects.
 */
void derive_ts_types(compass::model::ops::builder& b,
    compass::model::types::value_id entity,
    const types::entity_registry& registry);

/**
 * @brief The TypeScript interfaces and modules of the entity's groups
 * (core.py 6580-6608): ts_type and ts_import on each domain group, and
 * the identity and audit groups' pascal names and imports when the
 * entity has them.
 */
void derive_ts_group_imports(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief ts_column_imports (core.py 6609-6624): field_group_ts_imports
 * over the columns, less the interfaces the groups already import.
 */
void derive_ts_column_imports(compass::model::ops::builder& b,
    compass::model::types::value_id entity,
    const types::entity_registry& registry);

/**
 * @brief entity_struct_member_columns: the columns that reach the
 * entity's flat domain struct, in template order. None for a grouped
 * entity; the primary key and natural keys unless the identity group
 * carries them; then every column that is neither an identity-group
 * column nor sql_only.
 */
[[nodiscard]] std::vector<compass::model::types::value_id>
entity_struct_member_columns(const compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The economic declaration (core.py 6625-6640):
 * has_economic_declaration, economic_fields and has_economic_fields, as
 * entity_economic_declaration reads the struct's members, and, when it
 * declares, canonical_members (entity_canonical_members) and
 * has_canonical_members. A grouped entity declares an empty list.
 */
void derive_economic_declaration(compass::model::ops::builder& b,
    compass::model::types::value_id entity,
    const types::entity_registry& registry);

/**
 * @brief messages (core.py 6641-6647): entity_protocol_messages, the
 * derived records and operations followed by the declared messages, with
 * requires_session set on every message that states a subject. A
 * read-only entity keeps no write operation and no write-only record.
 * @p siblings are the entity singulars modelled beside the entity
 * (sibling_entity_singulars): when one is <singular>_version the versions
 * sub-resource takes the plural as its stem. The declared messages are
 * the very dicts the Python appends, so requires_session is set on them
 * in declared_messages too.
 */
void derive_messages(compass::model::ops::builder& b,
    compass::model::types::value_id entity,
    const types::entity_registry& registry,
    const std::set<std::string, std::less<>>& siblings);

/**
 * @brief The codec headers (core.py 6648-6656): wire_codec_includes, the
 * headers declaring the types a row holds (_codec_includes: the groups'
 * own headers, the audit group's, then _member_headers of the columns,
 * primary key columns and natural keys); message_codec_includes,
 * _member_headers of every message field; and has_generated_wire_codec,
 * from @p site. _require_visible_write_observer only refuses a model, so
 * it is not ported.
 */
void derive_codec_includes(compass::model::ops::builder& b,
    compass::model::types::value_id entity,
    const types::entity_registry& registry, const types::entity_site& site);

/**
 * @brief The filter record (core.py 6663-6684): filter_members as
 * filter_members derives them, search_columns as a C++ string list,
 * has_search, has_range_filter, has_filter_record (a record exists and
 * @p site generates its protocol), filter_type and payload_member
 * (response_payload_member).
 */
void derive_filter_record(compass::model::ops::builder& b,
    compass::model::types::value_id entity, const types::entity_site& site);

/**
 * @brief entity_ts_alias (core.py 6685-6691, apply_ts_domain_alias): when
 * a shared utility interface the messages import owns the entity's
 * TypeScript name, the alias <Pascal>Entity, which the ts_type of every
 * field naming the entity's own type follows. Nothing otherwise.
 */
void derive_ts_alias(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The key finders (core.py 6692-6711): key_finders, the legacy
 * service_find_by_code finder and a finder by the declared key when it is
 * not the storage key and the model states no such read itself, each
 * with the fragments _prepare_finder_render states, which
 * service_find_by_code gains too; and key_resolvers, the declared key's
 * read when it is not the storage key.
 */
void derive_key_finders(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The declared key (core.py 6712-6758): declared_key,
 * key_is_primary, history_by_row_id, declared_key_text and
 * declared_row_text (_key_as_text), is_declared_key_finder on each key
 * finder, declared_key_has_batch_read, declared_key_group_prefix and
 * declared_key_is_uuid.
 */
void derive_declared_key(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief The write record (core.py 6759-6768): write_fields
 * (write_record_for) with the enum and date conversions
 * _prepare_enum_write_fields and _prepare_date_write_fields state on them,
 * preserved_fields (service_preserved_fields) and has_preserved_fields.
 */
void derive_write_fields(compass::model::ops::builder& b,
    compass::model::types::value_id entity,
    const types::entity_registry& registry);

/**
 * @brief struct_name (core.py 6657-6662), the message's name again, on
 * every message. A declared message is the very dict the Python appends
 * to messages, so it is set on declared_messages too.
 */
void derive_message_struct_names(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief presentation.import_preview_columns, the columns with
 * import_preview set (org_loader.py's presentation parse). The Python's
 * list holds the very column objects, so the oracle shows them as the
 * presentation steps leave them: run after the last change to a column.
 */
void project_import_preview_columns(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

/**
 * @brief primary_key.extra_columns (core.py 5793), the key's columns
 * after the first. The Python's list holds the very column objects, so
 * the oracle shows them as later steps leave them: run after the last
 * change to a key column.
 */
void project_primary_key_extra_columns(compass::model::ops::builder& b,
    compass::model::types::value_id entity);

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

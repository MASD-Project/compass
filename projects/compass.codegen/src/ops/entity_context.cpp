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
#include "compass.codegen/ops/entity_registry.hpp"
#include "compass.codegen/ops/entity_steps.hpp"
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

using compass::model::types::value_id;
using compass::model::types::value_kind;

value_id entity_context(compass::model::ops::builder& b, value_id library,
    value_id model, std::string_view model_key,
    const types::entity_registry& registry, const types::entity_site& site) {
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
    derive_field_groups(b, entity);
    derive_group_prefixes(b, entity);
    derive_stamp_members(b, entity);
    derive_key_group_prefixes(b, entity);
    derive_supply(b, entity);
    derive_sample_tokens(b, entity);
    inject_group_includes(b, entity);
    derive_natural_key_flags(b, entity);
    derive_key_aggregates(b, entity);
    mark_index_and_validation_last_items(b, entity);
    format_description(b, entity);
    derive_component_names(b, entity);
    derive_entity_names(b, entity);
    derive_table_display(b, entity);
    copy_repository_fields(b, entity);
    derive_tenant_scope(b, entity);
    derive_messaging_defaults(b, entity);
    derive_list_requests(b, entity);
    apply_stated_order(b, entity);
    derive_foreign_key_seeds(b, entity, registry);
    derive_party_scope(b, entity);
    derive_storage_shape(b, entity);
    derive_sql_names(b, entity);
    resolve_pinned_keys(b, entity, registry);
    resolve_routed_check(b, entity, registry);
    derive_table_rules(b, entity);
    derive_primary_key(b, entity);
    derive_presentation_columns(b, entity);
    derive_presentation_detail_defaults(b, entity);
    derive_detail_fields(b, entity);
    derive_presentation_flags(b, entity);
    derive_generator_facet_names(b, entity);
    derive_uuid_column_flags(b, entity);
    derive_render_kind_flags(b, entity);
    derive_mapper_include_gates(b, entity);
    derive_uuid_include_gates(b, entity);
    derive_ts_types(b, entity, registry);
    derive_ts_group_imports(b, entity);
    derive_ts_column_imports(b, entity, registry);
    derive_economic_declaration(b, entity, registry);
    derive_messages(b, entity, registry,
        directory_of(registry, site.path).entity_singulars);
    derive_codec_includes(b, entity, registry, site);
    derive_message_struct_names(b, entity);
    derive_filter_record(b, entity, site);
    derive_ts_alias(b, entity);
    derive_key_finders(b, entity);
    derive_declared_key(b, entity);
    derive_write_fields(b, entity, registry);
    derive_operations(b, entity);
    derive_surface_plans(b, entity);
    derive_service_gates(b, entity, site);
    derive_events(b, entity);
    project_enforced_foreign_keys(b, entity);
    project_primary_key_extra_columns(b, entity);
    project_import_preview_columns(b, entity);
    project_sql_table(b, context, loaded, entity);
    b.set_copy(context, "domain_entity", entity);
    return context;
}

}

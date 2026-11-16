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
#ifndef COMPASS_CODEGEN_TYPES_ENTITY_REGISTRY_HPP
#define COMPASS_CODEGEN_TYPES_ENTITY_REGISTRY_HPP

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "compass.model/ops/typescript.hpp"

namespace compass::codegen::types {

/**
 * @brief A foreign key of a parent model that is not nullable, which a
 * seeded parent row needs a row of its own for.
 */
struct mandatory_foreign_key {
    std::string column;
    std::string table;
    std::string target_column;
    bool use_system_tenant = false;
};

/**
 * @brief What core.py's _parent_entity_info reads from a model as loaded,
 * with no enrichment: the facts a referencing entity's seeding, enforced
 * keys and pinned keys need about the table it references.
 *
 * column_prefixes maps a column to the member it is reached through
 * (_column_member_prefixes); unique_keys holds the column sets a foreign
 * key can reference (_unique_keys); column_names every declared column.
 */
struct parent_entity {
    std::string entity_singular;
    std::string generator_facet_name;
    std::string component;
    bool has_audit_group = false;
    bool has_identity_group = false;
    bool seed_country_sentinel = false;
    bool seed_currency = false;
    bool no_audit_columns = false;
    bool immutable = false;
    bool immutable_tenant_key = false;
    bool has_tenant_id = false;
    bool party_isolated = false;
    bool current_state = false;
    std::map<std::string, std::string, std::less<>> column_prefixes;
    std::vector<mandatory_foreign_key> mandatory_fks;
    std::vector<std::set<std::string>> unique_keys;
    std::set<std::string, std::less<>> column_names;
};

/**
 * @brief The entity model that states a table: the #+entity_singular: its
 * text names, and the model as loaded, or none when it does not load.
 */
struct table_owner {
    std::string entity_singular;
    std::optional<parent_entity> parent;
};

/**
 * @brief One row of a trade type catalogue: a code and the entity it is
 * routed to, empty when none.
 */
struct trade_type_route {
    std::string code;
    std::string instrument;
};

/**
 * @brief What a directory's .org models declare, by their #+type:
 * (_model_type_names and _entity_singulars_in): the entity singulars, the
 * entity and junction singulars, and the field group singulars.
 */
struct model_directory {
    std::set<std::string, std::less<>> entity_singulars;
    std::set<std::string, std::less<>> member_names;
    std::set<std::string, std::less<>> group_names;
};

/**
 * @brief What the enrichment reads about the model file itself: its path
 * relative to the checkout, whether it enables every wire-codec address
 * (_has_wire_codec) and whether it generates its protocol header
 * (_protocol_enabled), after its own overrides, and whether an operation
 * model beside it owns its protocol (_protocol_owned_by_operation).
 */
struct entity_site {
    std::string path;
    bool has_generated_wire_codec = false;
    bool protocol_enabled = true;
    bool protocol_owned = false;
};

/**
 * @brief One convention family, as convention_family_registry's
 * FamilyEntry states it: an entity model of ores.refdata that declares
 * #+convention_family:.
 */
struct convention_family {
    std::string key;
    std::string entity;
    std::string domain_type;
    std::string subject_stem;
    bool party_scoped = false;
    std::string write_mode;
    std::string component;
    bool has_wire_codec = false;
};

/**
 * @brief The models a domain entity's enrichment reads besides its own,
 * read once for every model.
 *
 * tables maps each :tablename: of a projects/<component>/modeling/ entity
 * model to its owner, as _entity_org_by_table and _parent_entity_info
 * resolve it. trade_type_routes maps the file stem of each trade type
 * catalogue there to its rows, in catalogue order. custom_types holds
 * the rows of the custom-type registry the TypeScript projections read.
 * economic_field_groups holds the dotted names of the field groups whose
 * models declare an economic field (_group_declares_economic_fields).
 * directories maps the directory of every .org model under a modeling/
 * directory of projects/ to what its models declare. domain_headers holds
 * the path of every header under a domain/ directory of projects/, which
 * _foreign_root asks about. convention_families holds the convention
 * families (discover_entries), by key.
 */
struct entity_registry {
    std::map<std::string, table_owner, std::less<>> tables;
    std::map<std::string, std::vector<trade_type_route>, std::less<>>
        trade_type_routes;
    compass::model::ops::custom_types custom_types;
    std::set<std::string, std::less<>> economic_field_groups;
    std::map<std::string, model_directory, std::less<>> directories;
    std::set<std::string, std::less<>> domain_headers;
    std::vector<convention_family> convention_families;
};

}

#endif

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
#include "compass.crosscheck/codecs/er_diagram.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

const std::string parser_path =
    "projects/ores.codegen/src/plantuml_er_parse_sql.py";
const std::string group_path =
    "projects/ores.codegen/library/templates/plantuml_er.mustache";
const std::string index_path =
    "projects/ores.codegen/library/templates/plantuml_er_index.mustache";
const std::string modeling = "projects/ores.sql/modeling/";

const std::string parser =
    "COMPONENT_PREFIXES = {\n"
    "    'ores_iam_': {'name': 'iam', 'description': 'Identity', "
    "'schema': 'public', 'color': '#E8F4FD', 'order': 2},\n"
    "    'ores_dq_': {'name': 'dq', 'description': 'Data Quality', "
    "'schema': 'public', 'color': '#E1F5FE', 'order': 4},\n"
    "    'ores_geo_': {'name': 'geo', 'description': 'Geolocation', "
    "'schema': 'public', 'color': '#FFF9C4', 'order': 1},\n"
    "}\n"
    "SCHEMA_PATTERN = r'(?:public)'\n"
    "FK_COLUMNS = {\n"
    "    'change_reason_code',  # References change reason codes\n"
    "    'country_code',        # References countries\n"
    "}\n"
    "TABLE_STYLES = {\n"
    "    'temporal': {'stereotype': '<<temporal>>', 'color': '#C6F0D8'},\n"
    "    'artefact': {'stereotype': '', 'color': '#F7E5FF'},\n"
    "    'junction': {'stereotype': '<<junction>>', 'color': '#ECECEC'},\n"
    "    'current_state': {'stereotype': '', 'color': '#F7E5FF'},\n"
    "}\n";

const std::string group_template =
    "{{! a comment }}\n"
    "title {{diagram_title}} {{group_table_count}} {{index_file}}\n"
    "{{#external_note}}\nother:\n{{external_note}}\n{{/external_note}}\n"
    "{{#packages}}\npackage {{name}} {{description}} {{color}}\n"
    "{{#tables}}\nentity {{name}} {{stereotype}} {{color}}\n"
    "{{#primary_key.columns}}\n  * {{name}} : {{type}}"
    "{{#is_fk}} FK{{/is_fk}}\n{{/primary_key.columns}}\n"
    "{{#columns}}\n  {{^nullable}}*{{/nullable}}{{name}} : {{type}}"
    "{{#is_unique}} U{{/is_unique}}{{#is_fk}} FK{{/is_fk}}\n{{/columns}}\n"
    "{{#description}}\n  note {{description}}\n{{/description}}\n"
    "{{/tables}}\n{{/packages}}\n"
    "{{#relationships}}\n{{from_table}} {{cardinality}} {{to_table}} : "
    "{{label}}\n{{/relationships}}\n"
    "source {{source_file}}\n";

const std::string index_template =
    "{{total_tables}} tables in {{package_count}} groups\n"
    "{{#packages}}\n{{name}}: {{table_count}} {{file}} {{description}}\n"
    "{{/packages}}\nsource {{source_file}}\n";

files tree(std::initializer_list<std::pair<const std::string, std::string>>
    scripts) {
    files out{scripts};
    out.emplace(parser_path, parser);
    out.emplace(group_path, group_template);
    out.emplace(index_path, index_template);
    return out;
}

const std::string countries =
    "-- ====\n-- Table: countries\n-- The countries of the world.\n"
    "-- ISO 3166.\n-- ====\n"
    "create table if not exists \"public\".\"ores_geo_countries_tbl\" (\n"
    "    \"tenant_id\" uuid not null,\n"
    "    \"alpha2_code\" text not null,\n"
    "    \"name\" character varying(100) not null,\n"
    "    \"valid_from\" timestamp with time zone not null,\n"
    "    \"valid_to\" timestamp with time zone not null,\n"
    "    primary key (tenant_id, alpha2_code, valid_from),\n"
    "    check (valid_from < valid_to)\n"
    ");\n"
    "create unique index if not exists x_uniq_idx\n"
    "on \"public\".\"ores_geo_countries_tbl\" (name, tenant_id) where "
    "valid_to = 'infinity';\n";

const std::string accounts =
    "/**\n * Accounts Table\n * Who may sign in.\n */\n"
    "create table if not exists ores_iam_accounts_tbl (\n"
    "    \"id\" uuid not null,\n"
    "    \"country_code\" text null,\n"
    "    \"change_reason_code\" text not null,\n"
    "    \"valid_from\" timestamptz not null,\n"
    "    \"valid_to\" timestamptz not null,\n"
    "    primary key (id, valid_from)\n"
    ");\n"
    "-- Validate country_code (soft FK to ores_geo_countries_tbl)\n"
    "create table if not exists ores_iam_account_roles_tbl (\n"
    "    \"account_id\" uuid not null,\n"
    "    \"role_id\" uuid not null references ores_iam_accounts_tbl(id),\n"
    "    \"valid_from\" timestamptz not null,\n"
    "    \"valid_to\" timestamptz not null,\n"
    "    primary key (account_id, role_id, valid_from)\n"
    ");\n"
    "-- Validate account_id (soft FK to ores_iam_accounts_tbl)\n";

const std::string datasets =
    "create table if not exists ores_dq_datasets_tbl (\n"
    "    \"id\" uuid not null,\n"
    "    primary key (id)\n);\n"
    "create table if not exists ores_dq_countries_artefact_tbl (\n"
    "    \"dataset_id\" uuid not null,\n"
    "    \"code\" text\n);\n"
    "-- Validate dataset_id (soft FK to ores_dq_datasets_tbl)\n";

}

TEST_CASE("each group gets its diagram and the index names them, in the "
    "order of the components", "[codecs][er_diagram]") {
    const auto out = tests::generate(codecs::er_diagram, tree({
        {"projects/ores.sql/create/geo/geo_countries_create.sql", countries},
        {"projects/ores.sql/create/iam/iam_accounts_create.sql", accounts},
        {"projects/ores.sql/create/dq/dq_datasets_create.sql", datasets}}));
    CHECK(out.at(modeling + "ores_schema.puml") ==
        "5 tables in 4 groups\n"
        "geo: 1 ores_schema.geo.puml Geolocation\n"
        "iam: 2 ores_schema.iam.puml Identity\n"
        "dq_staging: 1 ores_schema.dq_staging.puml Data Staging\n"
        "dq_datasets: 1 ores_schema.dq_datasets.puml Dataset Management\n"
        "source ores_schema.puml\n");
    CHECK(out.at(modeling + "ores_schema.geo.puml") ==
        "title geo 1 ores_schema.puml\n"
        "other:\n- ores_iam_accounts_tbl (iam)\n"
        "package geo Geolocation #FFF9C4\n"
        "entity ores_geo_countries_tbl <<temporal>> #C6F0D8\n"
        "  * tenant_id : uuid FK\n"
        "  * alpha2_code : text\n"
        "  *name : character varying(100) U\n"
        "  *valid_from : timestamptz\n"
        "  *valid_to : timestamptz\n"
        "  note The countries of the world.\n    ISO 3166.\n"
        "source ores_schema.geo.puml\n");
    CHECK(out.at(modeling + "ores_schema.iam.puml") ==
        "title iam 2 ores_schema.puml\n"
        "other:\n- ores_geo_countries_tbl (geo)\n"
        "package iam Identity #E8F4FD\n"
        "entity ores_iam_account_roles_tbl <<junction>> #ECECEC\n"
        "  * account_id : uuid FK\n"
        "  * role_id : uuid FK\n"
        "  * valid_from : timestamptz\n"
        "  *valid_to : timestamptz\n"
        "entity ores_iam_accounts_tbl <<temporal>> #C6F0D8\n"
        "  * id : uuid\n"
        "  country_code : text FK\n"
        "  *change_reason_code : text FK\n"
        "  *valid_from : timestamptz\n"
        "  *valid_to : timestamptz\n"
        "  note Who may sign in.\n"
        "ores_iam_accounts_tbl ||--o{ ores_iam_account_roles_tbl : assigned\n"
        "source ores_schema.iam.puml\n");
    CHECK(out.at(modeling + "ores_schema.dq_staging.puml") ==
        "title dq_staging 1 ores_schema.puml\n"
        "other:\n- ores_dq_datasets_tbl (dq_datasets)\n"
        "package dq_staging Data Staging #F3E5F5\n"
        "entity ores_dq_countries_artefact_tbl  #F7E5FF\n"
        "  *dataset_id : uuid FK\n"
        "  code : text\n"
        "source ores_schema.dq_staging.puml\n");
}

TEST_CASE("a key a column of two tables of one script could own draws no "
    "edge, tables of earlier scripts do not compete, and an edge is drawn "
    "once", "[codecs][er_diagram]") {
    const auto out = tests::generate(codecs::er_diagram, tree({
        {"projects/ores.sql/create/geo/geo_a_create.sql",
         "create table if not exists ores_geo_parents_tbl (\n"
         "    \"id\" uuid not null\n);\n"
         "create table if not exists ores_geo_children_tbl (\n"
         "    \"parent_id\" uuid not null,\n"
         "    \"second_parent_id\" uuid not null,\n"
         "    \"other_id\" uuid not null\n);\n"
         "create table if not exists ores_geo_others_tbl (\n"
         "    \"other_id\" uuid not null\n);\n"
         "create table if not exists ores_geo_extras_tbl (\n"
         "    \"id\" uuid not null\n);\n"
         "-- Validate parent_id (soft FK to ores_geo_parents_tbl)\n"
         "-- Validate second_parent_id (optional soft FK to "
         "ores_geo_parents_tbl)\n"
         "-- Validate other_id (soft FK to ores_geo_extras_tbl)\n"},
        {"projects/ores.sql/create/geo/geo_b_create.sql",
         "create table if not exists ores_geo_more_tbl (\n"
         "    \"other_id\" uuid not null\n);\n"
         "-- Validate other_id (soft FK to ores_geo_extras_tbl)\n"}}));
    const auto& geo = out.at(modeling + "ores_schema.geo.puml");
    CHECK(geo.contains(
        "ores_geo_parents_tbl ||--o{ ores_geo_children_tbl : has\n"
        "ores_geo_extras_tbl ||--o{ ores_geo_more_tbl : has\n"
        "source"));
}

TEST_CASE("nothing is written when the parser's tables are not literals",
    "[codecs][er_diagram]") {
    auto t = tree({{"projects/ores.sql/create/geo/geo_countries_create.sql",
        countries}});
    t[parser_path] = "COMPONENT_PREFIXES = build_prefixes()\n";
    CHECK(tests::generate(codecs::er_diagram, t).empty());
}

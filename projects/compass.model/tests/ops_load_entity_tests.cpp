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
#include <map>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "load_fixture.hpp"

/*
 * The expected value is what ores.codegen's read_physical_space_overrides
 * returned for the same document, bound to ORE Studio's read-model profile.
 */
TEST_CASE("a model's own physical space table wins over its profile's",
    "[model][physical_space]") {
    compass::model::tests::checkout c;
    c.add("projects/modeling/variability_read_model.org",
        "#+title: variability_read_model\n\n* Physical space\n\n"
        "| Address | Enabled |\n|---------+---------|\n"
        "| ores.sql.schema.domain_entity_create | false |\n"
        "| ores.sql.schema.domain_entity_drop | false |\n"
        "| ores.sql.schema.domain_entity_view_create | true |\n"
        "| ores.sql.schema.domain_entity_view_drop | true |\n"
        "| ores.sql.schema.notify_trigger | false |\n"
        "| ores.sql.schema.notify_trigger_drop | false |\n");
    c.add("projects/ores.demo/modeling/ores.demo.view.org",
        "#+title: x\n#+type: ores.codegen.entity\n\n"
        "* Flags\n:PROPERTIES:\n:profile: read-model\n:END:\n\n"
        "* Physical space\n\n| Address | Enabled |\n|---------+---------|\n"
        "| =ores.sql.schema.notify_trigger= | TRUE |\n"
        "| ores.cpp.service | false |\n"
        "| ores.cpp.protocol | maybe |\n");

    CHECK(c.overrides("projects/ores.demo/modeling/ores.demo.view.org") ==
        std::map<std::string, bool>{
            {"ores.sql.schema.domain_entity_create.enabled", false},
            {"ores.sql.schema.domain_entity_drop.enabled", false},
            {"ores.sql.schema.domain_entity_view_create.enabled", true},
            {"ores.sql.schema.domain_entity_view_drop.enabled", true},
            {"ores.sql.schema.notify_trigger.enabled", true},
            {"ores.sql.schema.notify_trigger_drop.enabled", false},
            {"ores.cpp.service.enabled", false}});
}

/*
 * The expected value is what ores.codegen.core.load_model returned for the
 * same document.
 */
TEST_CASE("an entity loads as ores.codegen loads it", "[model][entity]") {
    compass::model::tests::checkout c;
    c.add_modeling_documents();
    c.add("projects/ores.demo/modeling/ores.demo.widget.org", R"org(:PROPERTIES:
:ID: 0A1B2C3D-0000-4000-8000-000000000001
:END:
#+title: ores.demo.widget
#+type: ores.codegen.entity
#+brief: A widget.
#+entity_singular: widget
#+entity_plural: widgets
#+has_tenant_id: true

A widget the =demo= component keeps. It spans
two lines.

* Flags
:PROPERTIES:
:schema:    public
:product:   ores
:component: demo
:subcomponent: api
:profile:   simple-lookup
:END:

* Columns

** code
:PROPERTIES:
:type:        text
:cpp_type:    std::string
:primary_key: true
:END:

The widget code.

More about the code.

#+begin_src cpp :name generator
std::string("W")
#+end_src

** name
:PROPERTIES:
:type:        text
:cpp_type:    std::string
:natural_key: true
:END:

The widget name.

** schedule
:PROPERTIES:
:type:     text
:cpp_type: std::optional<cron_expression>
:nullable: true
:default:  it's
:END:

When it runs.

** kind
:PROPERTIES:
:type:          text
:cpp_type:      widget_kind
:is_enum:       true
:default_value: plain
:END:

* SQL

** Flags
:PROPERTIES:
:tablename: ores_demo_widgets_tbl
:END:

** Checks

| expression     |
|----------------|
| code <> ''     |

** Indexes

| name     | columns | unique |
|----------+---------+--------|
| by_name  | name    | true   |

* Foreign keys

** owner_id
:PROPERTIES:
:table:         ores_iam_accounts_tbl
:error_message: Owner isn't known
:list_by_as_of: true
:END:

* C++

** Flags
:PROPERTIES:
:service_find_by_code_column: code name
:END:

** Repository
:PROPERTIES:
:entity_singular_short: widget
:END:

** Domain includes

#+begin_src cpp :name includes
#include <string>
// a comment
#+end_src

** Entity includes

#+begin_src cpp :name includes
#include <string>
#include "ores.demo.api/domain/widget_kind.hpp"
#+end_src

** Table display

| column | header |
|--------+--------|
| code   | Code   |

** Presentation
:PROPERTIES:
:key_field:        code
:flag_accessor:    flag
:END:

*** Detail fields

| field | label | type         | combo_values |
|-------+-------+--------------+--------------|
| code  | Code  | static_combo | A, B,,C      |

*** Columns

| enum_name | field | header | type   | width | import_preview |
|-----------+-------+--------+--------+-------+----------------|
| Code      | code  | Code   | string | auto  | true           |
| Name      | name  | Name   | int    | 70    |                |

** Custom repository methods

*** find_newest
:PROPERTIES:
:ID: 0A1B2C3D-0000-4000-8000-000000000002
:END:

Finds the newest widget.

#+begin_src cpp :name declaration
widget find_newest();
#+end_src

* Extra

#+begin_src sql :name body :implements 5E47F108-1350-4540-B3C2-E83DD5379B2D
drop table if exists x;
#+end_src
)org");
    CHECK(c.load_json("projects/ores.demo/modeling/ores.demo.widget.org") == R"json({
  "domain_entity": {
    "brief": "A widget.",
    "columns": [
      {
        "cpp_type": "std::optional<cron_expression>",
        "default": "'it''s'",
        "description": "When it runs.",
        "name": "schedule",
        "nullable": true,
        "type": "text"
      },
      {
        "cpp_type": "widget_kind",
        "default_value": "widget_kind::plain",
        "is_enum": true,
        "name": "kind",
        "type": "text"
      }
    ],
    "component": "demo",
    "cpp": {
      "includes": {
        "domain": [
          "<string>",
          "\"ores.scheduler.api/domain/cron_expression.hpp\""
        ],
        "entity": [
          "\"ores.demo.api/domain/widget_kind.hpp\""
        ]
      },
      "iterator_var": "e",
      "table_display": [
        {
          "column": "code",
          "header": "Code"
        }
      ]
    },
    "custom_repository_methods": [
      {
        "declaration": "widget find_newest();",
        "description": "Finds the newest widget.",
        "id": "0A1B2C3D-0000-4000-8000-000000000002",
        "name": "find_newest"
      }
    ],
    "description": "A widget the demo component keeps. It spans\ntwo lines.",
    "entity_org_id": "0A1B2C3D-0000-4000-8000-000000000001",
    "entity_plural": "widgets",
    "entity_singular": "widget",
    "foreign_keys": [
      {
        "column": "owner_id",
        "error_message": "Owner isn''t known",
        "exists_function": "ores_iam_account_exists_fn",
        "list_by_as_of": true,
        "table": "ores_iam_accounts_tbl",
        "target_column": "id",
        "target_no_validity": false
      }
    ],
    "has_tenant_id": true,
    "implementations": {
      "5E47F108-1350-4540-B3C2-E83DD5379B2D": [
        "drop table if exists x;"
      ]
    },
    "indexes": [
      {
        "columns": "name",
        "current_only": false,
        "name": "by_name",
        "nulls_not_distinct": false,
        "unique": true,
        "using": null,
        "where_extra": null
      }
    ],
    "natural_keys": [
      {
        "column": "name",
        "cpp_type": "std::string",
        "description": "The widget name.",
        "type": "text"
      }
    ],
    "presentation": {
      "columns": [
        {
          "enum_name": "Code",
          "field": "code",
          "header": "Code",
          "import_preview": true,
          "is_string": true,
          "width": "auto"
        },
        {
          "enum_name": "Name",
          "field": "name",
          "header": "Name",
          "import_preview": "",
          "is_int": true,
          "width": 70
        }
      ],
      "detail_fields": [
        {
          "combo_values": [
            {
              "label": "A",
              "value": "A"
            },
            {
              "label": "B",
              "value": "B"
            },
            {
              "label": "C",
              "value": "C"
            }
          ],
          "field": "code",
          "label": "Code",
          "type": "static_combo"
        }
      ],
      "flag_accessor": "flag",
      "has_any_flag_icon": false,
      "has_change_reason_cache": true,
      "has_combo_flag_source": false,
      "has_dynamic_combo_helper_fields": false,
      "has_flag_icon": false,
      "has_generate_action": false,
      "has_key_flag_icon": false,
      "has_pagination": true,
      "has_toolbar": false,
      "has_uuid_primary_key": false,
      "import_preview_columns": [
        {
          "enum_name": "Code",
          "field": "code",
          "header": "Code",
          "import_preview": true,
          "is_string": true,
          "width": "auto"
        }
      ],
      "key_field": "code",
      "key_flag_accessor": "flag",
      "needs_image_cache": false
    },
    "primary_key": {
      "column": "code",
      "columns": [
        {
          "column": "code",
          "cpp_type": "std::string",
          "description": "The widget code.",
          "detail": "More about the code.",
          "generator_expr": "std::string(\"W\")",
          "type": "text"
        }
      ],
      "cpp_type": "std::string",
      "description": "The widget code.",
      "detail": "More about the code.",
      "generator_expr": "std::string(\"W\")",
      "type": "text"
    },
    "product": "ores",
    "profile": "simple-lookup",
    "repository": {
      "entity_singular_short": "widget"
    },
    "schema": "public",
    "service_find_by_code": {
      "column": "code",
      "columns": [
        "code",
        "name"
      ]
    },
    "sql": {
      "extra_checks": [
        "code <> ''"
      ],
      "has_list_by_as_of": true,
      "nullable_tenant_id": false,
      "system_scope": false,
      "tablename": "ores_demo_widgets_tbl"
    },
    "subcomponent": "api"
  }
}
)json");
}

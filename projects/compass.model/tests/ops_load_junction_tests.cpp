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
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "load_fixture.hpp"

/*
 * The expected value is what ores.codegen.core.load_model returned for the
 * same document.
 */
TEST_CASE("a junction loads as ores.codegen loads it", "[model][junction]") {
    compass::model::tests::checkout c;
    c.add_modeling_documents();
    c.add("projects/ores.demo/modeling/ores.demo.widget_tag_junction.org", R"org(#+title: ores.demo.widget_tag_junction
#+type: ores.codegen.junction
#+product: ores
#+schema: public
#+component: demo
#+name: widget_tags
#+name_singular: widget_tag
#+brief: Tags on widgets.

Which tags a widget carries.

* Flags
:PROPERTIES:
:has_tenant_id: true
:END:

* Left
:PROPERTIES:
:column:   widget_id
:type:     uuid
:cpp_type: boost::uuids::uuid
:END:

The widget.

* Right
:PROPERTIES:
:column:   tag_code
:type:     text
:cpp_type: std::string
:END:

* Columns

** weight
:PROPERTIES:
:type:     integer
:cpp_type: int
:default:  1
:END:

* Repository
:PROPERTIES:
:name_short: tags
:END:

* C++

** Flags
:PROPERTIES:
:subcomponent: api
:read_only:    true
:END:

** Presentation
:PROPERTIES:
:key_field: widget_id
:END:

* Artefact columns

** widget_id
:PROPERTIES:
:type: uuid
:END:
)org");
    CHECK(c.load_json("projects/ores.demo/modeling/ores.demo.widget_tag_junction.org") == R"json({
  "junction": {
    "artefact_columns": [
      {
        "name": "widget_id",
        "type": "uuid"
      }
    ],
    "artefact_key_column": "widget_id",
    "artefact_left_indexed": true,
    "artefact_right_indexed": false,
    "brief": "Tags on widgets.",
    "columns": [
      {
        "cpp_type": "int",
        "default": "1",
        "name": "weight",
        "type": "integer"
      }
    ],
    "component": "demo",
    "description": "Which tags a widget carries.",
    "entity_pascal": "WidgetTag",
    "entity_pascal_short": "Tag",
    "entity_pascal_short_plural": "Tags",
    "entity_plural": "widget_tags",
    "entity_plural_words": "tags",
    "entity_singular": "widget_tag",
    "entity_singular_words": "tag",
    "entity_snake": "widget_tag",
    "entity_title": null,
    "entity_upper": "WIDGET_TAG",
    "has_tenant_id": true,
    "left": {
      "column": "widget_id",
      "cpp_type": "boost::uuids::uuid",
      "description": "The widget.",
      "is_date": false,
      "is_timestamp": false,
      "is_uuid": true,
      "query_needs_str": true,
      "type": "uuid"
    },
    "name": "widget_tags",
    "name_singular": "widget_tag",
    "presentation": {
      "has_any_flag_icon": false,
      "has_combo_flag_source": false,
      "has_dynamic_combo_helper_fields": false,
      "has_flag_icon": false,
      "has_generate_action": false,
      "has_key_flag_icon": false,
      "has_toolbar": false,
      "key_field": "widget_id",
      "needs_image_cache": false
    },
    "product": "ores",
    "read_only": true,
    "repository": {
      "entity_plural_short": "tags",
      "entity_plural_words": "tags",
      "entity_singular_short": "widget_tag",
      "entity_singular_words": "tag",
      "name_short": "tags"
    },
    "right": {
      "column": "tag_code",
      "cpp_type": "std::string",
      "is_date": false,
      "is_timestamp": false,
      "is_uuid": false,
      "query_needs_str": false,
      "type": "text"
    },
    "schema": "public",
    "subcomponent": "api",
    "wire_write_enabled": false
  }
}
)json");
}

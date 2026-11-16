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
TEST_CASE("a field group loads as ores.codegen loads it", "[model][field_group]") {
    compass::model::tests::checkout c;
    c.add_modeling_documents();
    c.add("projects/ores.demo/modeling/ores.demo.limits_field_group.org", R"org(#+title: ores.demo.limits_field_group
#+type: ores.codegen.field_group
#+product: ores
#+component: demo
#+subcomponent: api
#+entity_singular: limits
#+brief: The limits a widget carries.
#+namespace: ores::demo::domain
#+header: domain/limits.hpp
#+export_macro: ORES_DEMO_API

Limits shared by every widget.

* Includes

#+begin_src cpp :name includes
#include <cstdint>
#include <string>
#+end_src

* Fields

** max_items
:PROPERTIES:
:cpp_type:      std::int64_t
:default_value: 0
:economic:      true
:END:

The most items a widget holds.
Second line.

** label
:PROPERTIES:
:cpp_type: std::string
:economic: false
:secret:   yes
:END:
)org");
    CHECK(c.load_json("projects/ores.demo/modeling/ores.demo.limits_field_group.org") == R"json({
  "field_group": {
    "brief": "The limits a widget carries.",
    "canonical_members": [
      "max_items"
    ],
    "component": "demo",
    "cpp": {
      "includes": [
        "<cstdint>",
        "<string>"
      ],
      "namespace": "ores::demo::domain"
    },
    "description": "Limits shared by every widget.",
    "economic_fields": [
      "max_items"
    ],
    "entity_singular": "limits",
    "export_macro": "ORES_DEMO_API",
    "fields": [
      {
        "cpp_type": "std::int64_t",
        "default_value": "0",
        "description": "The most items a widget holds.\nSecond line.",
        "economic": true,
        "name": "max_items"
      },
      {
        "cpp_type": "std::string",
        "economic": false,
        "name": "label",
        "secret": "yes"
      }
    ],
    "has_canonical_members": true,
    "has_economic_declaration": true,
    "has_economic_fields": true,
    "has_options_secrets": true,
    "has_printer": true,
    "header": "domain/limits.hpp",
    "header_guard": "ORES_DEMO_API_DOMAIN_LIMITS_HPP",
    "options_fields": [
      {
        "cpp_type": "std::int64_t",
        "default_value": "0",
        "description": "The most items a widget holds.\nSecond line.",
        "doc_lines": [
          "@brief The most items a widget holds.",
          "Second line."
        ],
        "economic": true,
        "has_doc": true,
        "name": "max_items",
        "secret": false
      },
      {
        "cpp_type": "std::string",
        "doc_lines": [],
        "economic": false,
        "has_doc": false,
        "name": "label",
        "secret": true
      }
    ],
    "options_secret_count": 1,
    "product": "ores",
    "subcomponent": "api"
  }
}
)json");
}

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
TEST_CASE("an outcome catalogue loads as ores.codegen loads it", "[model][outcome_catalogue]") {
    compass::model::tests::checkout c;
    c.add_modeling_documents();
    c.add("projects/ores.database/modeling/ores.demo.outcomes.org", R"org(#+title: ores.demo.outcomes
#+type: ores.codegen.outcome_catalogue
#+component: database
#+brief: Refusals.

* Outcomes

** not_found
:PROPERTIES:
:outcome:  missing
:producer: store
:sqlstate: P0002
:args:     entity, value
:END:

The row named is not there.

#+begin_src text :name message
No {entity} '{value}' exists.
#+end_src

** bad_request
:PROPERTIES:
:outcome:  invalid
:producer: request
:END:

#+begin_src text :name message
The request is "bad".
#+end_src
)org");
    CHECK(c.load_json("projects/ores.database/modeling/ores.demo.outcomes.org") == R"json({
  "outcome_catalogue": {
    "brief": "Refusals.",
    "component": "database",
    "outcomes": [
      {
        "arg_names": [
          "entity",
          "value"
        ],
        "args": [
          {
            "comma": ",",
            "name": "entity"
          },
          {
            "comma": "",
            "name": "value"
          }
        ],
        "class": "missing",
        "code": "not_found",
        "comma": ",",
        "description": "The row named is not there.",
        "is_request": false,
        "is_store": true,
        "last": false,
        "message": "No {entity} '{value}' exists.",
        "message_cpp": "No {entity} '{value}' exists.",
        "message_sql": "No {entity} ''{value}'' exists.",
        "producer": "store",
        "sqlstate": "P0002",
        "sqlstate_quoted": "'P0002'",
        "summary": "The row named is not there."
      },
      {
        "arg_names": [],
        "args": [],
        "class": "invalid",
        "code": "bad_request",
        "comma": "",
        "description": "",
        "is_request": true,
        "is_store": false,
        "last": true,
        "message": "The request is \"bad\".",
        "message_cpp": "The request is \\\"bad\\\".",
        "message_sql": "The request is \"bad\".",
        "producer": "request",
        "sqlstate": "",
        "sqlstate_quoted": "",
        "summary": ""
      }
    ],
    "raiser_args": [
      {
        "comma": ",",
        "name": "entity"
      },
      {
        "comma": "",
        "name": "value"
      }
    ],
    "request_outcomes": [
      {
        "arg_names": [],
        "args": [],
        "class": "invalid",
        "code": "bad_request",
        "comma": "",
        "description": "",
        "is_request": true,
        "is_store": false,
        "last": true,
        "message": "The request is \"bad\".",
        "message_cpp": "The request is \\\"bad\\\".",
        "message_sql": "The request is \"bad\".",
        "producer": "request",
        "sqlstate": "",
        "sqlstate_quoted": "",
        "summary": ""
      }
    ],
    "store_outcomes": [
      {
        "arg_names": [
          "entity",
          "value"
        ],
        "args": [
          {
            "comma": ",",
            "name": "entity"
          },
          {
            "comma": "",
            "name": "value"
          }
        ],
        "class": "missing",
        "code": "not_found",
        "comma": ",",
        "description": "The row named is not there.",
        "is_request": false,
        "is_store": true,
        "last": false,
        "message": "No {entity} '{value}' exists.",
        "message_cpp": "No {entity} '{value}' exists.",
        "message_sql": "No {entity} ''{value}'' exists.",
        "producer": "store",
        "sqlstate": "P0002",
        "sqlstate_quoted": "'P0002'",
        "summary": "The row named is not there."
      }
    ]
  }
}
)json");
}

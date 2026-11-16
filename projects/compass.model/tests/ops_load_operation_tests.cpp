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
TEST_CASE("an operation loads as ores.codegen loads it", "[model][operation]") {
    compass::model::tests::checkout c;
    c.add_modeling_documents();
    c.add("projects/ores.demo/modeling/ores.demo.widget_operations.org", R"org(:PROPERTIES:
:ores.cpp.shell-command.enabled: true
:END:
#+title: ores.demo.widget_operations
#+type: ores.codegen.operation
#+product: ores
#+component: demo
#+subcomponent: api
#+entity_singular: widget_run
#+namespace: ores::demo::messaging
#+brief: Operations on widgets.

Runs a widget.

* Includes

#+begin_src cpp :name includes
#include <vector>
#+end_src

* Messages

** run_widget_request
:PROPERTIES:
:subject:  demo.v1.widgets.run
:response: run_widget_response
:destructive: yes
:request_timeout_seconds: 30
:shell_command: run
:END:

#+begin_src cpp :name comment
/**
 * @brief Runs one widget.
 */
#+end_src

*** code
:PROPERTIES:
:cpp_type: std::string
:END:

*** owner
:PROPERTIES:
:cpp_type: std::optional<boost::uuids::uuid>
:default:  std::nullopt
:END:

#+begin_src cpp :name comment
/**
 * @brief Who runs it.
 */
#+end_src

*** tags
:PROPERTIES:
:cpp_type: std::vector<std::string>
:default:  {}
:END:

** run_widget_response
:PROPERTIES:
:END:

*** result
:PROPERTIES:
:cpp_type: ores::utility::domain::result
:END:

*** widget
:PROPERTIES:
:cpp_type: std::vector<ores::other::domain::widget>
:END:

** status_request
:PROPERTIES:
:subject:  {component}.v1.widgets.status
:response: status_response
:auth:     none
:rate_policy: public
:END:

** status_response
:PROPERTIES:
:END:

*** up
:PROPERTIES:
:cpp_type: bool
:END:
)org");
    CHECK(c.load_json("projects/ores.demo/modeling/ores.demo.widget_operations.org") == R"json({
  "operation": {
    "brief": "Operations on widgets.",
    "component": "demo",
    "derived_subject": true,
    "derived_subject_prefix": "",
    "derived_subject_suffix": ".v1.widgets.status",
    "description": "Runs a widget.",
    "domain_imports": [
      {
        "entity": "widget",
        "entity_pascal": "Widget",
        "module_dir": "../../other/domain"
      }
    ],
    "entity_singular": "widget_run",
    "includes": [
      "<vector>"
    ],
    "messages": [
      {
        "comment": "/**\n * @brief Runs one widget.\n */",
        "destructive": true,
        "fields": [
          {
            "comment": "",
            "cpp_type": "std::string",
            "name": "code",
            "ts_type": "string"
          },
          {
            "comment": "    /**\n     * @brief Who runs it.\n     */",
            "cpp_type": "std::optional<boost::uuids::uuid>",
            "default": "std::nullopt",
            "name": "owner",
            "ts_type": "string | null"
          },
          {
            "comment": "",
            "cpp_type": "std::vector<std::string>",
            "default": "{}",
            "name": "tags",
            "ts_type": "string[]"
          }
        ],
        "name": "run_widget_request",
        "name_pascal": "RunWidgetRequest",
        "request_timeout_seconds": 30,
        "requires_session": "true",
        "response_type": "run_widget_response",
        "shell_command": "run",
        "subject": "demo.v1.widgets.run"
      },
      {
        "fields": [
          {
            "comment": "",
            "cpp_type": "ores::utility::domain::result",
            "name": "result",
            "ts_type": "Result"
          },
          {
            "comment": "",
            "cpp_type": "std::vector<ores::other::domain::widget>",
            "name": "widget",
            "ts_type": "Widget[]"
          }
        ],
        "name": "run_widget_response",
        "name_pascal": "RunWidgetResponse"
      },
      {
        "fields": [],
        "name": "status_request",
        "name_pascal": "StatusRequest",
        "rate_policy": "public",
        "requires_session": "false",
        "response_type": "status_response",
        "subject": "{component}.v1.widgets.status",
        "subject_is_derived": true
      },
      {
        "fields": [
          {
            "comment": "",
            "cpp_type": "bool",
            "name": "up",
            "ts_type": "boolean"
          }
        ],
        "name": "status_response",
        "name_pascal": "StatusResponse"
      }
    ],
    "namespace": "ores::demo::messaging",
    "product": "ores",
    "shell_command_count": 1,
    "shell_commands": [
      {
        "adopts_token_field": null,
        "command": "run",
        "flags": [
          {
            "cpp_type": "std::optional<boost::uuids::uuid>",
            "default": "std::nullopt",
            "fillable": true,
            "is_bool": false,
            "is_list": false,
            "is_number": false,
            "is_optional": true,
            "is_string": false,
            "name": "owner",
            "needs_from_token": true,
            "token_literal": "00000000-0000-0000-0000-000000000001"
          },
          {
            "cpp_type": "std::vector<std::string>",
            "default": "{}",
            "fillable": true,
            "is_bool": false,
            "is_list": true,
            "is_number": false,
            "is_optional": true,
            "is_string": false,
            "name": "tags",
            "needs_from_token": false,
            "token_literal": "sample"
          }
        ],
        "has_flags": true,
        "has_positionals": true,
        "http_route": false,
        "identifier": "run",
        "invocation": "run __none__",
        "is_destructive": true,
        "operation": "run-widget",
        "operation_identifier": "run_widget",
        "positional_count": 1,
        "positionals": [
          {
            "cpp_type": "std::string",
            "default": null,
            "fillable": true,
            "is_bool": false,
            "is_list": false,
            "is_number": false,
            "is_optional": false,
            "is_string": true,
            "name": "code",
            "needs_from_token": false,
            "token_literal": "sample"
          }
        ],
        "public": false,
        "rate_policy": "",
        "request": "run_widget_request",
        "request_timeout_seconds": 30,
        "response_type": "run_widget_response",
        "subject": "demo.v1.widgets.run",
        "unsupported": [],
        "usage": "run <code> [--owner <v>] [--tags <v>]"
      }
    ],
    "shell_has_bool": false,
    "shell_has_helpers": true,
    "shell_has_list": true,
    "shell_has_request_timeout": true,
    "subcomponent": "api",
    "subject_function": "widget_run_subject_for",
    "subject_function_by_component": "widget_run_subject_by_component",
    "subject_function_ts": "widgetRunSubjectFor",
    "utility_imports": [
      {
        "module": "utility/protocol",
        "name_pascal": "Result"
      }
    ]
  }
}
)json");
}

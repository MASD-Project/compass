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

namespace {

using compass::model::tests::checkout;

/*
 * The expected values are what ores.codegen.core.load_model returned for
 * the same document.
 */
const std::string overview = R"org(:PROPERTIES:
:ID: 11111111-2222-3333-4444-555555555555
:END:
#+title: ores.demo
#+description: A demo component   with =spaces=.
#+type: ores.codegen.component
#+name: demo
#+full_name: ores.demo
#+brief: Demo brief.
#+component_kind: composite
#+parts: api core service
#+header_only: yes
#+test_fixture: none

Prose for humans.

* Build

#+begin_src cmake :name extra_dependencies
find_package(Boost REQUIRED)
#+end_src

* Service

#+begin_src cpp :implements FDEE629F-F8BA-4940-AA1B-6671AF332D82
int x = 1;
#+end_src

* Options
:PROPERTIES:
:group: Demo "options"
:END:

** port
:PROPERTIES:
:cpp_type: int
:default:  8080
:help:     The port   to listen on.
:secret:   nil
:END:

The port the service listens on.
Second line.

** api_key
:PROPERTIES:
:cpp_type: std::string
:default:  ""
:help:     The key.
:secret:   t
:positive: yes
:option:   key
:END:
)org";

}

TEST_CASE("a component overview loads as ores.codegen loads it",
    "[model][component]") {
    checkout c;
    c.add("projects/ores.demo/modeling/component_overview.org", overview);
    CHECK(c.load_json("projects/ores.demo/modeling/component_overview.org") ==
        R"json({
  "component": {
    "brief": "Demo brief.",
    "description": "A demo component with =spaces=.",
    "extra_dependencies": "find_package(Boost REQUIRED)",
    "extra_test_dependencies": "",
    "full_name": "ores.demo",
    "has_positive_options": true,
    "has_service_options": true,
    "has_service_secrets": true,
    "has_test_fixture": false,
    "header_only": true,
    "hidden_visibility": false,
    "implementations": {
      "FDEE629F-F8BA-4940-AA1B-6671AF332D82": [
        "int x = 1;"
      ]
    },
    "kind": "composite",
    "name": "demo",
    "parts": [
      {
        "last": false,
        "part": "api"
      },
      {
        "last": false,
        "part": "core"
      },
      {
        "last": true,
        "part": "service"
      }
    ],
    "service_options": [
      {
        "cpp_type": "int",
        "default": "8080",
        "doc_lines": [
          "@brief The port the service listens on.",
          "Second line."
        ],
        "help": "The port to listen on.",
        "initial": "",
        "name": "port",
        "option": "port",
        "positive": false,
        "secret": false
      },
      {
        "cpp_type": "std::string",
        "default": "\"\"",
        "doc_lines": [
          "@brief api_key"
        ],
        "help": "The key.",
        "initial": "",
        "name": "api_key",
        "option": "key",
        "positive": true,
        "secret": true
      }
    ],
    "service_options_group": "Demo \\\"options\\\"",
    "service_secret_count": 1,
    "win32_static_library": false
  }
}
)json");
}

TEST_CASE("a component model that is not an overview takes its description "
    "from its prose", "[model][component]") {
    checkout c;
    c.add("projects/ores.demo/modeling/demo_component.org", overview);
    CHECK(c.load_json("projects/ores.demo/modeling/demo_component.org") ==
        R"json({
  "component": {
    "brief": "Demo brief.",
    "description": "Prose for humans.",
    "full_name": "ores.demo",
    "name": "demo"
  }
}
)json");
}

TEST_CASE("an option with a flag that is neither yes nor no is refused",
    "[model][component]") {
    checkout c;
    c.add("projects/ores.demo/modeling/component_overview.org",
        "#+type: ores.codegen.component\n\n* Options\n:PROPERTIES:\n"
        ":group: G\n:END:\n** port\n:PROPERTIES:\n:cpp_type: int\n"
        ":default: 1\n:help: h\n:secret: maybe\n:END:\n");
    CHECK(c.load_json("projects/ores.demo/modeling/component_overview.org") ==
        "{\n  \"error\": \"ValueError\"\n}\n");
}

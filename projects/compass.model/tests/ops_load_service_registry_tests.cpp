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
TEST_CASE("a service registry loads as ores.codegen loads it", "[model][service_registry]") {
    compass::model::tests::checkout c;
    c.add_modeling_documents();
    c.add("projects/modeling/service_registry.org", R"org(#+title: service_registry
#+type: ores.codegen.service_registry

* ores.demo.service
:PROPERTIES:
:psql_var:     demo_service
:replicas:     2
:enabled:      False
:depends_on:   ores.iam.service, ores.nats
:description:  The demo service.
:END:

** DML prefixes

- ores_demo_
- ores_misc_

** Select tables

- schema=public, table=ores_iam_accounts_tbl
- junk

** Extra args

- --verbose

* Not a service

Prose only.
)org");
    CHECK(c.load_json("projects/modeling/service_registry.org") == R"json({
  "service_registry": {
    "services": [
      {
        "depends_on": [
          "ores.iam.service",
          "ores.nats"
        ],
        "description": "The demo service.",
        "dml_prefixes": [
          {
            "prefix": "ores_demo_"
          },
          {
            "prefix": "ores_misc_"
          }
        ],
        "enabled": false,
        "execute_prefixes": [],
        "extra_args": [
          "--verbose"
        ],
        "name": "ores.demo.service",
        "psql_var": "demo_service",
        "replicas": 2,
        "select_prefixes": [],
        "select_tables": [
          {
            "schema": "public",
            "table": "ores_iam_accounts_tbl"
          }
        ]
      }
    ]
  }
}
)json");
}

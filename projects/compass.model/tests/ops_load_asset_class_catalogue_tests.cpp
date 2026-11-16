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
TEST_CASE("an asset class catalogue loads as ores.codegen loads it", "[model][asset_class_catalogue]") {
    compass::model::tests::checkout c;
    c.add_modeling_documents();
    c.add("projects/ores.refdata/modeling/ores.demo.asset_class_catalogue.org", R"org(#+title: ores.demo.asset_class_catalogue
#+type: ores.codegen.asset_class_catalogue
#+component: refdata
#+brief: Asset classes.

* Taxonomy

** rates
:PROPERTIES:
:name:          Interest Rates
:display_order: 3
:badge_css:     blue
:END:

Rates and the curves they're built on.

** fx
:PROPERTIES:
:name: FX
:END:

Foreign exchange.

* Namespace

** IR
:PROPERTIES:
:refdata_code: rates
:END:

Interest rate data.

** Other
:PROPERTIES:
:END:

* Code domain

** asset_class
:PROPERTIES:
:name:          Asset class
:display_order: 5
:END:

The classes: {codes}.
)org");
    CHECK(c.load_json("projects/ores.refdata/modeling/ores.demo.asset_class_catalogue.org") == R"json({
  "asset_class_catalogue": {
    "authority_to_class": {
      "IR": "rates",
      "Other": ""
    },
    "brief": "Asset classes.",
    "code_domain": {
      "code": "asset_class",
      "description": "The classes: rates, fx.",
      "description_sql": "The classes: rates, fx.",
      "display_order": 5,
      "name": "Asset class"
    },
    "component": "refdata",
    "namespace": [
      {
        "authority": "IR",
        "description": "Interest rate data.",
        "refdata_code": "rates"
      },
      {
        "authority": "Other",
        "description": "",
        "refdata_code": ""
      }
    ],
    "taxonomy": [
      {
        "badge_background": "",
        "badge_code": "asset_class_rates",
        "badge_css": "blue",
        "badge_description": "Interest Rates asset class.",
        "badge_name": "Interest Rates",
        "badge_order": 0,
        "badge_severity": "",
        "badge_text": "",
        "code": "rates",
        "comma": ",",
        "description": "Rates and the curves they're built on.",
        "description_sql": "Rates and the curves they''re built on.",
        "display_order": 3,
        "last": false,
        "name": "Interest Rates"
      },
      {
        "badge_background": "",
        "badge_code": "asset_class_fx",
        "badge_css": "",
        "badge_description": "FX asset class.",
        "badge_name": "FX",
        "badge_order": 0,
        "badge_severity": "",
        "badge_text": "",
        "code": "fx",
        "comma": "",
        "description": "Foreign exchange.",
        "description_sql": "Foreign exchange.",
        "display_order": 2,
        "last": true,
        "name": "FX"
      }
    ]
  }
}
)json");
}

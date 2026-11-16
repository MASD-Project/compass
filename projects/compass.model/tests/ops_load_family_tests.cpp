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
 * The expected values are what ores.codegen.core.load_model returned for
 * ORE Studio's ores.refdata.tenor_basis_swap_convention: its frontmatter,
 * central entity, parts and cardinality view. The messages and imports
 * that load_org_family_model derives from the central entity are derived
 * by compass.codegen's family context instead.
 */
TEST_CASE("a family loads its parts and their cardinality",
    "[model][family]") {
    compass::model::tests::checkout c;
    c.add_modeling_documents();
    c.add("projects/ores.refdata/modeling/x.org", R"org(#+title: x
#+type: ores.codegen.family
#+component: refdata
#+subcomponent: api
#+entity_singular: tenor_basis_swap_convention

A tenor basis swap.

* Flags
:PROPERTIES:
:central_entity: convention
:END:

* Parts

| part        | entity                       | cardinality | group |
|-------------+------------------------------+-------------+-------|
| pay_receive | tenor_basis_swap_pay_receive | one_of      | form  |
| long_short  | tenor_basis_swap_long_short  | one_of      | form  |
| extra       | tenor_basis_swap_extra       | optional    |       |
)org");
    CHECK(c.load_json("projects/ores.refdata/modeling/x.org") == R"json({
  "family": {
    "cardinality_groups": [
      {
        "label": "pay_receive or long_short",
        "terms": [
          {
            "last": false,
            "part": "pay_receive"
          },
          {
            "last": true,
            "part": "long_short"
          }
        ]
      }
    ],
    "cardinality_one": [],
    "cardinality_optional": [
      {
        "part": "extra"
      }
    ],
    "central_entity": "convention",
    "component": "refdata",
    "description": "A tenor basis swap.",
    "entity_singular": "tenor_basis_swap_convention",
    "parts": [
      {
        "cardinality": "one_of",
        "entity": "tenor_basis_swap_pay_receive",
        "group": "form",
        "part": "pay_receive"
      },
      {
        "cardinality": "one_of",
        "entity": "tenor_basis_swap_long_short",
        "group": "form",
        "part": "long_short"
      },
      {
        "cardinality": "optional",
        "entity": "tenor_basis_swap_extra",
        "group": "",
        "part": "extra"
      }
    ],
    "subcomponent": "api"
  }
}
)json");
}

TEST_CASE("a family without a central entity is refused",
    "[model][family]") {
    compass::model::tests::checkout c;
    c.add_modeling_documents();
    c.add("projects/ores.refdata/modeling/x.org", R"org(#+title: x
#+type: ores.codegen.family
#+component: refdata
)org");
    CHECK(c.load_json("projects/ores.refdata/modeling/x.org") ==
        "{\n  \"error\": \"ValueError\"\n}\n");
}

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

constexpr auto family_path = "projects/ores.refdata/modeling/x.org";

/*
 * A family and the entities beside it: the central convention declares
 * its key as the natural key name, and one part entity backs two parts.
 */
compass::model::tests::checkout refdata_checkout() {
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
| extra       | tenor_basis_swap_pay_receive | optional    |       |
)org");
    c.add("projects/ores.refdata/modeling/ores.refdata.convention.org", R"org(#+title: ores.refdata.convention
#+type: ores.codegen.entity
#+component: refdata
#+entity_singular: convention

* Columns

** id
:PROPERTIES:
:type:        uuid
:cpp_type:    boost::uuids::uuid
:primary_key: true
:END:

** name
:PROPERTIES:
:type:        text
:natural_key: true
:END:

* C++

** Presentation
:PROPERTIES:
:key_field: name
:END:
)org");
    c.add("projects/ores.refdata/modeling/ores.refdata.tenor_basis_swap_pay_receive.org", R"org(#+title: ores.refdata.tenor_basis_swap_pay_receive
#+type: ores.codegen.entity
#+component: refdata
#+entity_singular: tenor_basis_swap_pay_receive

* Columns

** id
:PROPERTIES:
:type:        uuid
:cpp_type:    boost::uuids::uuid
:primary_key: true
:END:

** convention_id
:PROPERTIES:
:type:     uuid
:cpp_type: boost::uuids::uuid
:END:

* Foreign keys

** convention_id
:PROPERTIES:
:table:               ores_refdata_conventions_tbl
:parent_entity:       convention
:list_by:             true
:list_by_uuid:        true
:bump_parent_version: true
:list_by_as_of:       true
:END:
)org");
    c.add("projects/ores.refdata/modeling/ores.refdata.tenor_basis_swap_long_short.org", R"org(#+title: ores.refdata.tenor_basis_swap_long_short
#+type: ores.codegen.entity
#+component: refdata
#+entity_singular: tenor_basis_swap_long_short

* Columns

** id
:PROPERTIES:
:type:        uuid
:cpp_type:    boost::uuids::uuid
:primary_key: true
:END:

** convention_id
:PROPERTIES:
:type:     uuid
:cpp_type: boost::uuids::uuid
:END:

* Foreign keys

** convention_id
:PROPERTIES:
:table:               ores_refdata_conventions_tbl
:parent_entity:       convention
:list_by:             true
:list_by_uuid:        true
:bump_parent_version: true
:list_by_as_of:       true
:END:
)org");

    return c;
}

}

/*
 * The expected value is what ores.codegen's load_org_family_model returned
 * for the same documents.
 */
TEST_CASE("a family loads its parts and the protocol of its central row",
    "[model][family]") {
    const auto c = refdata_checkout();
    CHECK(c.load_json(family_path) == R"json({
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
    "domain_imports": [
      {
        "entity": "convention",
        "entity_pascal": "Convention",
        "module_dir": "../domain"
      },
      {
        "entity": "tenor_basis_swap_long_short",
        "entity_pascal": "TenorBasisSwapLongShort",
        "module_dir": "../domain"
      },
      {
        "entity": "tenor_basis_swap_pay_receive",
        "entity_pascal": "TenorBasisSwapPayReceive",
        "module_dir": "../domain"
      }
    ],
    "domain_includes": [
      "ores.refdata.api/domain/convention.hpp",
      "ores.refdata.api/domain/tenor_basis_swap_pay_receive.hpp",
      "ores.refdata.api/domain/tenor_basis_swap_long_short.hpp"
    ],
    "entity_singular": "tenor_basis_swap_convention",
    "item_fields": [
      {
        "comment": "",
        "cpp_type": "tenor_basis_swap_convention_key",
        "name": "key",
        "ts_type": "TenorBasisSwapConventionKey"
      },
      {
        "comment": "",
        "cpp_type": "std::optional<ores::refdata::domain::convention>",
        "name": "convention",
        "ts_type": "Convention | null"
      },
      {
        "comment": "",
        "cpp_type": "std::vector<ores::refdata::domain::tenor_basis_swap_pay_receive>",
        "name": "pay_receive",
        "ts_type": "TenorBasisSwapPayReceive[]"
      },
      {
        "comment": "",
        "cpp_type": "std::vector<ores::refdata::domain::tenor_basis_swap_long_short>",
        "name": "long_short",
        "ts_type": "TenorBasisSwapLongShort[]"
      },
      {
        "comment": "",
        "cpp_type": "std::vector<ores::refdata::domain::tenor_basis_swap_pay_receive>",
        "name": "extra",
        "ts_type": "TenorBasisSwapPayReceive[]"
      }
    ],
    "key_fields": [
      {
        "comment": "",
        "cpp_type": "std::string",
        "name": "name",
        "ts_type": "string"
      }
    ],
    "messages": [
      {
        "comment": "",
        "derived": true,
        "fields": [
          {
            "comment": "",
            "cpp_type": "std::string",
            "name": "name",
            "ts_type": "string"
          }
        ],
        "name": "tenor_basis_swap_convention_key",
        "name_pascal": "TenorBasisSwapConventionKey"
      },
      {
        "comment": "",
        "derived": true,
        "fields": [
          {
            "comment": "",
            "cpp_type": "tenor_basis_swap_convention_key",
            "name": "key",
            "ts_type": "TenorBasisSwapConventionKey"
          },
          {
            "comment": "",
            "cpp_type": "std::optional<ores::refdata::domain::convention>",
            "name": "convention",
            "ts_type": "Convention | null"
          },
          {
            "comment": "",
            "cpp_type": "std::vector<ores::refdata::domain::tenor_basis_swap_pay_receive>",
            "name": "pay_receive",
            "ts_type": "TenorBasisSwapPayReceive[]"
          },
          {
            "comment": "",
            "cpp_type": "std::vector<ores::refdata::domain::tenor_basis_swap_long_short>",
            "name": "long_short",
            "ts_type": "TenorBasisSwapLongShort[]"
          },
          {
            "comment": "",
            "cpp_type": "std::vector<ores::refdata::domain::tenor_basis_swap_pay_receive>",
            "name": "extra",
            "ts_type": "TenorBasisSwapPayReceive[]"
          }
        ],
        "name": "tenor_basis_swap_convention_item",
        "name_pascal": "TenorBasisSwapConventionItem"
      },
      {
        "comment": "",
        "derived": true,
        "fields": [
          {
            "comment": "",
            "cpp_type": "convention_change",
            "name": "change",
            "ts_type": "ConventionChange"
          },
          {
            "comment": "",
            "cpp_type": "std::vector<tenor_basis_swap_pay_receive_change>",
            "name": "pay_receive",
            "ts_type": "TenorBasisSwapPayReceiveChange[]"
          },
          {
            "comment": "",
            "cpp_type": "std::vector<tenor_basis_swap_long_short_change>",
            "name": "long_short",
            "ts_type": "TenorBasisSwapLongShortChange[]"
          },
          {
            "comment": "",
            "cpp_type": "std::vector<tenor_basis_swap_pay_receive_change>",
            "name": "extra",
            "ts_type": "TenorBasisSwapPayReceiveChange[]"
          }
        ],
        "name": "tenor_basis_swap_convention_write",
        "name_pascal": "TenorBasisSwapConventionWrite"
      },
      {
        "comment": "",
        "derived": true,
        "fields": [
          {
            "comment": "",
            "cpp_type": "tenor_basis_swap_convention_key",
            "name": "key",
            "ts_type": "TenorBasisSwapConventionKey"
          }
        ],
        "name": "get_tenor_basis_swap_convention_request",
        "name_pascal": "GetTenorBasisSwapConventionRequest",
        "requires_session": "true",
        "response_type": "get_tenor_basis_swap_convention_response",
        "subject": "refdata.v1.tenor_basis_swap_conventions.get"
      },
      {
        "comment": "",
        "derived": true,
        "fields": [
          {
            "comment": "",
            "cpp_type": "ores::utility::domain::result",
            "name": "result",
            "ts_type": "Result"
          },
          {
            "comment": "",
            "cpp_type": "std::optional<tenor_basis_swap_convention_item>",
            "name": "item",
            "ts_type": "TenorBasisSwapConventionItem | null"
          }
        ],
        "name": "get_tenor_basis_swap_convention_response",
        "name_pascal": "GetTenorBasisSwapConventionResponse"
      },
      {
        "comment": "",
        "derived": true,
        "fields": [
          {
            "comment": "",
            "cpp_type": "std::vector<tenor_basis_swap_convention_key>",
            "name": "keys",
            "ts_type": "TenorBasisSwapConventionKey[]"
          }
        ],
        "name": "get_many_tenor_basis_swap_conventions_request",
        "name_pascal": "GetManyTenorBasisSwapConventionsRequest",
        "requires_session": "true",
        "response_type": "get_many_tenor_basis_swap_conventions_response",
        "subject": "refdata.v1.tenor_basis_swap_conventions.get_many"
      },
      {
        "comment": "",
        "derived": true,
        "fields": [
          {
            "comment": "",
            "cpp_type": "ores::utility::domain::result",
            "name": "result",
            "ts_type": "Result"
          },
          {
            "comment": "",
            "cpp_type": "std::vector<tenor_basis_swap_convention_item>",
            "name": "items",
            "ts_type": "TenorBasisSwapConventionItem[]"
          }
        ],
        "name": "get_many_tenor_basis_swap_conventions_response",
        "name_pascal": "GetManyTenorBasisSwapConventionsResponse"
      },
      {
        "comment": "",
        "derived": true,
        "fields": [
          {
            "comment": "",
            "cpp_type": "tenor_basis_swap_convention_write",
            "name": "item",
            "ts_type": "TenorBasisSwapConventionWrite"
          },
          {
            "comment": "",
            "cpp_type": "ores::utility::domain::change_intent",
            "name": "intent",
            "ts_type": "ChangeIntent"
          }
        ],
        "name": "put_tenor_basis_swap_convention_request",
        "name_pascal": "PutTenorBasisSwapConventionRequest",
        "requires_session": "true",
        "response_type": "put_tenor_basis_swap_convention_response",
        "subject": "refdata.v1.tenor_basis_swap_conventions.put"
      },
      {
        "comment": "",
        "derived": true,
        "fields": [
          {
            "comment": "",
            "cpp_type": "ores::utility::domain::result",
            "name": "result",
            "ts_type": "Result"
          },
          {
            "comment": "",
            "cpp_type": "std::optional<tenor_basis_swap_convention_item>",
            "name": "item",
            "ts_type": "TenorBasisSwapConventionItem | null"
          }
        ],
        "name": "put_tenor_basis_swap_convention_response",
        "name_pascal": "PutTenorBasisSwapConventionResponse"
      },
      {
        "comment": "",
        "derived": true,
        "fields": [
          {
            "comment": "",
            "cpp_type": "std::vector<tenor_basis_swap_convention_write>",
            "name": "items",
            "ts_type": "TenorBasisSwapConventionWrite[]"
          },
          {
            "comment": "",
            "cpp_type": "ores::utility::domain::change_intent",
            "name": "intent",
            "ts_type": "ChangeIntent"
          }
        ],
        "name": "put_many_tenor_basis_swap_conventions_request",
        "name_pascal": "PutManyTenorBasisSwapConventionsRequest",
        "requires_session": "true",
        "response_type": "put_many_tenor_basis_swap_conventions_response",
        "subject": "refdata.v1.tenor_basis_swap_conventions.put_many"
      },
      {
        "comment": "",
        "derived": true,
        "fields": [
          {
            "comment": "",
            "cpp_type": "ores::utility::domain::result",
            "name": "result",
            "ts_type": "Result"
          },
          {
            "comment": "",
            "cpp_type": "std::vector<tenor_basis_swap_convention_item>",
            "name": "items",
            "ts_type": "TenorBasisSwapConventionItem[]"
          }
        ],
        "name": "put_many_tenor_basis_swap_conventions_response",
        "name_pascal": "PutManyTenorBasisSwapConventionsResponse"
      }
    ],
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
        "entity": "tenor_basis_swap_pay_receive",
        "group": "",
        "part": "extra"
      }
    ],
    "protocol_imports": [
      {
        "entity": "convention",
        "entity_pascal": "Convention"
      },
      {
        "entity": "tenor_basis_swap_pay_receive",
        "entity_pascal": "TenorBasisSwapPayReceive"
      },
      {
        "entity": "tenor_basis_swap_long_short",
        "entity_pascal": "TenorBasisSwapLongShort"
      }
    ],
    "protocol_includes": [
      "ores.refdata.api/messaging/convention_protocol.hpp",
      "ores.refdata.api/messaging/tenor_basis_swap_pay_receive_protocol.hpp",
      "ores.refdata.api/messaging/tenor_basis_swap_long_short_protocol.hpp"
    ],
    "subcomponent": "api",
    "utility_imports": [
      {
        "module": "utility/protocol",
        "name_pascal": "ChangeIntent"
      },
      {
        "module": "utility/protocol",
        "name_pascal": "Result"
      }
    ],
    "write_fields": [
      {
        "comment": "",
        "cpp_type": "convention_change",
        "name": "change",
        "ts_type": "ConventionChange"
      },
      {
        "comment": "",
        "cpp_type": "std::vector<tenor_basis_swap_pay_receive_change>",
        "name": "pay_receive",
        "ts_type": "TenorBasisSwapPayReceiveChange[]"
      },
      {
        "comment": "",
        "cpp_type": "std::vector<tenor_basis_swap_long_short_change>",
        "name": "long_short",
        "ts_type": "TenorBasisSwapLongShortChange[]"
      },
      {
        "comment": "",
        "cpp_type": "std::vector<tenor_basis_swap_pay_receive_change>",
        "name": "extra",
        "ts_type": "TenorBasisSwapPayReceiveChange[]"
      }
    ]
  }
}
)json");
}

TEST_CASE("a family without a central entity is refused",
    "[model][family]") {
    compass::model::tests::checkout c;
    c.add_modeling_documents();
    c.add(family_path, R"org(#+title: x
#+type: ores.codegen.family
#+component: refdata
)org");
    CHECK(c.load_json(family_path) == "{\n  \"error\": \"ValueError\"\n}\n");
}

TEST_CASE("a family naming a part no entity beside it declares is refused",
    "[model][family]") {
    auto c = refdata_checkout();
    c.add("projects/ores.refdata/modeling/"
        "ores.refdata.tenor_basis_swap_long_short.org",
        "#+title: gone\n#+type: ores.codegen.entity\n");
    CHECK(c.load_json(family_path) == "{\n  \"error\": \"ValueError\"\n}\n");
}

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
#include "compass.model/ops/entity_claim.hpp"
#include "load_fixture.hpp"

namespace {

constexpr auto catalogue_path =
    "projects/ores.trading/modeling/ores.trading.trade_type_catalogue.org";

/*
 * A catalogue and the entities of its component: two routed headers, a
 * child of the batch and a shared common date. bond_instrument's cascade
 * names a table no entity claims; rate_instrument states its plural and
 * declares * Domain groups rather than :economic:.
 */
compass::model::tests::checkout trading_checkout() {
    compass::model::tests::checkout c;
    c.add("projects/ores.trading/modeling/ores.trading.trade_type_catalogue.org", R"org(#+title: ores.trading.trade_type_catalogue
#+type: ores.codegen.trade_type_catalogue
#+component: trading
#+brief: Trade types.

* Trade types

| code        | name            | family  | construction | markets        | has_options | has_extension | instrument      |
|-------------+-----------------+---------+--------------+----------------+-------------+---------------+-----------------|
| Swap        | Interest  Swap  | swap    | native       | rates!, credit | false       | false         | rate_instrument |
| Bond        | Bond            |         | native       |                | true        | false         | bond_instrument |
| ForwardBond | Forward Bond    | forward | scripted     | rates!         | false       | true          | bond_instrument |
| Exotic      | Banker's Exotic |         | script       |                | false       | false         |                 |

* Descriptions

| code        | description             | source         | source_ref |
|-------------+-------------------------+----------------+------------|
| Swap        | An  interest rate swap. | ORE User Guide | swap.tex   |
| Bond        | A bond's cash flows.    | ORE User Guide | bond.tex   |
| ForwardBond | A forward on a bond.    | ORE User Guide | fwd.tex    |
| Exotic      | An exotic.              | ORE User Guide | exo.tex    |

* Classes

| code | deal_class |
|------+------------|
| Swap | flow       |

* Unsupported trade types

| code   | reason                         |
|--------+--------------------------------|
| Exotic | ORE has no builder for Exotic. |

* Batch

** Instrument types

| instrument      | cpp_type             |
|-----------------+----------------------|
| bond_instrument | bond_instrument_data |

** Children

| child    |
|----------|
| bond_leg |

* Digest

** Instrument components

| instrument      | component       |
|-----------------+-----------------|
| bond_instrument | bond_instrument |
| rate_instrument | rate_instrument |
| rate_instrument | bond_leg        |

** Shared components

| component     | reader                     |
|---------------+----------------------------|
| trade_booking | read_bookings_by_trade_ids |

* Common dates

| name          | description       | shared_entity | shared_column |
|---------------+-------------------+---------------+---------------|
| trade_date    | The date  agreed. | trade_booking | trade_date    |
| start_date    | The start.        |               |               |
| maturity_date | The end.          |               |               |
)org");
    c.add("projects/ores.trading/modeling/ores.trading.bond_instrument.org", R"org(#+title: ores.trading.bond_instrument
#+type: ores.codegen.entity
#+entity_singular: bond_instrument

* SQL
:PROPERTIES:
:tablename: ores_trading_bond_instruments_tbl
:END:

* Columns

** trade_id
:PROPERTIES:
:primary_key: true
:END:

** maturity_date
:PROPERTIES:
:economic: true
:common_date: maturity_date
:END:

* Delete cascade

** legs
:PROPERTIES:
:table: ores_trading_bond_legs_tbl
:END:

** notes
:PROPERTIES:
:table: ores_trading_notes_tbl
:END:
)org");
    c.add("projects/ores.trading/modeling/ores.trading.bond_leg.org", R"org(#+title: ores.trading.bond_leg
#+type: ores.codegen.entity
#+entity_singular: bond_leg

* SQL
:PROPERTIES:
:tablename: ores_trading_bond_legs_tbl
:END:

* Columns

** trade_id
:PROPERTIES:
:primary_key: True
:END:

** start_date
:PROPERTIES:
:economic: false
:common_date: start_date
:END:
)org");
    c.add("projects/ores.trading/modeling/ores.trading.rate_instrument.org", R"org(#+title: ores.trading.rate_instrument
#+type: ores.codegen.entity
#+entity_singular: rate_instrument
#+entity_plural: rate_instrument_rows

* SQL
:PROPERTIES:
:tablename: ores_trading_rate_instruments_tbl
:END:

* Domain groups

* Columns

** trade_id
:PROPERTIES:
:primary_key: true
:END:
)org");
    c.add("projects/ores.trading/modeling/ores.trading.trade_booking.org", R"org(#+title: ores.trading.trade_booking
#+type: ores.codegen.entity
#+entity_singular: trade_booking

* SQL
:PROPERTIES:
:tablename: ores_trading_trade_bookings_tbl
:END:

* Columns

** trade_id
:PROPERTIES:
:primary_key: true
:END:

** trade_date
:PROPERTIES:
:economic: true
:END:
)org");
    return c;
}

}

/*
 * The expected value is what ores.codegen's
 * load_org_trade_type_catalogue_model returned for the same documents.
 */
TEST_CASE("a trade type catalogue loads as ores.codegen loads it",
    "[model][trade_type_catalogue]") {
    const auto c = trading_checkout();
    CHECK(c.load_json(catalogue_path) == R"json({
  "trade_type_catalogue": {
    "batch": [
      {
        "comma": ",",
        "cpp_type": "bond_instrument_data",
        "entity": "bond_instrument",
        "name": "bond_instruments",
        "routed": true,
        "ts_module": "./domain/bond_instrument_data.js",
        "ts_pascal": "BondInstrumentData"
      },
      {
        "comma": ",",
        "cpp_type": "rate_instrument",
        "entity": "rate_instrument",
        "name": "rate_instrument_rows",
        "routed": true,
        "ts_module": "./domain/rate_instrument.js",
        "ts_pascal": "RateInstrument"
      },
      {
        "comma": "",
        "cpp_type": "bond_leg",
        "entity": "bond_leg",
        "name": "bond_legs",
        "routed": false,
        "ts_module": "./domain/bond_leg.js",
        "ts_pascal": "BondLeg"
      }
    ],
    "brief": "Trade types.",
    "common_dates": {
      "names": [
        {
          "description": "The date agreed.",
          "name": "trade_date",
          "shared": true
        },
        {
          "description": "The start.",
          "name": "start_date",
          "shared": false
        },
        {
          "description": "The end.",
          "name": "maturity_date",
          "shared": false
        }
      ],
      "views": [
        {
          "columns": [
            {
              "comma": ",",
              "expr": "j2.trade_date",
              "name": "trade_date"
            },
            {
              "comma": ",",
              "expr": "j1.start_date",
              "name": "start_date"
            },
            {
              "comma": "",
              "expr": "h.maturity_date",
              "name": "maturity_date"
            }
          ],
          "entity": "bond_instrument",
          "header_table": "ores_trading_bond_instruments_tbl",
          "joins": [
            {
              "alias": "j1",
              "table": "ores_trading_bond_legs_tbl"
            },
            {
              "alias": "j2",
              "table": "ores_trading_trade_bookings_tbl"
            }
          ],
          "mapped": [
            {
              "entity_column": "bond_instrument.maturity_date",
              "name": "maturity_date"
            },
            {
              "entity_column": "bond_leg.start_date",
              "name": "start_date"
            }
          ],
          "view": "ores_trading_bond_instruments_common_dates_vw"
        }
      ]
    },
    "component": "trading",
    "digest": [
      {
        "components": [
          {
            "entity": "bond_instrument"
          }
        ],
        "instrument": "bond_instrument"
      },
      {
        "components": [
          {
            "entity": "rate_instrument"
          },
          {
            "entity": "bond_leg"
          }
        ],
        "instrument": "rate_instrument"
      }
    ],
    "digest_shared": [
      {
        "entity": "trade_booking",
        "reader": "read_bookings_by_trade_ids"
      }
    ],
    "instruments": [
      {
        "codes": [
          "Bond",
          "ForwardBond"
        ],
        "comma": ",",
        "name": "bond_instrument"
      },
      {
        "codes": [
          "Swap"
        ],
        "comma": "",
        "name": "rate_instrument"
      }
    ],
    "market_rows": [
      {
        "asset_class_code": "rates",
        "comma": ",",
        "is_home": "true",
        "product_type_code": "Swap"
      },
      {
        "asset_class_code": "credit",
        "comma": ",",
        "is_home": "false",
        "product_type_code": "Swap"
      },
      {
        "asset_class_code": "rates",
        "comma": "",
        "is_home": "true",
        "product_type_code": "ForwardBond"
      }
    ],
    "routes": [
      {
        "code": "Swap",
        "instrument": "rate_instrument"
      },
      {
        "code": "Bond",
        "instrument": "bond_instrument"
      },
      {
        "code": "ForwardBond",
        "instrument": "bond_instrument"
      }
    ],
    "rows": [
      {
        "code": "Swap",
        "comma": ",",
        "construction": "native",
        "deal_class": "flow",
        "deal_class_literal": "'flow'",
        "description_sql": "An interest rate swap.",
        "display_order": 1,
        "family": "swap",
        "family_literal": "'swap'",
        "has_extension": "false",
        "has_options": "false",
        "instrument": "rate_instrument",
        "instrument_literal": "'rate_instrument'",
        "markets": [
          {
            "code": "rates",
            "is_home": true
          },
          {
            "code": "credit",
            "is_home": false
          }
        ],
        "name_sql": "Interest Swap",
        "source_ref_sql": "swap.tex",
        "source_sql": "ORE User Guide"
      },
      {
        "code": "Bond",
        "comma": ",",
        "construction": "native",
        "deal_class": "non_flow",
        "deal_class_literal": "'non_flow'",
        "description_sql": "A bond''s cash flows.",
        "display_order": 2,
        "family": "",
        "family_literal": "null",
        "has_extension": "false",
        "has_options": "true",
        "instrument": "bond_instrument",
        "instrument_literal": "'bond_instrument'",
        "markets": [],
        "name_sql": "Bond",
        "source_ref_sql": "bond.tex",
        "source_sql": "ORE User Guide"
      },
      {
        "code": "ForwardBond",
        "comma": ",",
        "construction": "scripted",
        "deal_class": "non_flow",
        "deal_class_literal": "'non_flow'",
        "description_sql": "A forward on a bond.",
        "display_order": 3,
        "family": "forward",
        "family_literal": "'forward'",
        "has_extension": "true",
        "has_options": "false",
        "instrument": "bond_instrument",
        "instrument_literal": "'bond_instrument'",
        "markets": [
          {
            "code": "rates",
            "is_home": true
          }
        ],
        "name_sql": "Forward Bond",
        "source_ref_sql": "fwd.tex",
        "source_sql": "ORE User Guide"
      },
      {
        "code": "Exotic",
        "comma": "",
        "construction": "script",
        "deal_class": "non_flow",
        "deal_class_literal": "'non_flow'",
        "description_sql": "An exotic.",
        "display_order": 4,
        "family": "",
        "family_literal": "null",
        "has_extension": "false",
        "has_options": "false",
        "instrument": "",
        "instrument_literal": "null",
        "markets": [],
        "name_sql": "Banker''s Exotic",
        "source_ref_sql": "exo.tex",
        "source_sql": "ORE User Guide"
      }
    ],
    "unsupported": [
      {
        "code": "Exotic",
        "reason": "ORE has no builder for Exotic."
      }
    ]
  }
}
)json");
}

TEST_CASE("a digest component no entity model declares is refused",
    "[model][trade_type_catalogue]") {
    auto c = trading_checkout();
    c.add("projects/ores.trading/modeling/ores.trading.trade_booking.org",
        "#+title: ores.trading.trade_booking\n");
    CHECK(c.load_json(catalogue_path) ==
        "{\n  \"error\": \"ValueError\"\n}\n");
}

TEST_CASE("an org file claims the first table name after its singular",
    "[model][entity_claim]") {
    const auto claim = compass::model::ops::entity_claim_of(
        "#+title: x\n#+entity_singular: currency\n* SQL\n"
        ":PROPERTIES:\n:tablename:     ores_refdata_currencies_tbl\n:END:\n");
    REQUIRE(claim);
    CHECK(claim->table == "ores_refdata_currencies_tbl");
    CHECK(claim->entity_singular == "currency");
    CHECK(claim->entity_plural == "currencys");
    CHECK_FALSE(compass::model::ops::entity_claim_of(
        "A doc naming =:tablename:= x_tbl\n:tablename: x_tbl\n"));
    CHECK_FALSE(compass::model::ops::entity_claim_of(
        "#+entity_singular: doc\nthe =:tablename:= drawer\n"));
}

TEST_CASE("an org file claims its stated plural",
    "[model][entity_claim]") {
    const auto claim = compass::model::ops::entity_claim_of(
        "#+entity_singular: index\n#+entity_plural: indices\n"
        ":tablename: ores_x_indices_tbl_v2\n");
    REQUIRE(claim);
    CHECK(claim->table == "ores_x_indices_tbl");
    CHECK(claim->entity_plural == "indices");
}

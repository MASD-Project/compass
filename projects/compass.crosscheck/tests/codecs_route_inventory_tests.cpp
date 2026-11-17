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
#include "compass.crosscheck/codecs/route_inventory.hpp"

#include <string>
#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

const std::string inventory_path =
    "doc/knowledge/architecture/http_route_inventory.org";

// route_inventory.py --write was run on this fixture; the expected text is
// what it wrote after its header.
const std::string file0 = R"f(from dataclasses import dataclass

RESOURCE = "resource"
COMPOSED = "composed view"
NOT_API = "not an API"

KINDS = (RESOURCE, COMPOSED, NOT_API)

NO_AREA = "(none)"

AREAS = (
    "access",
    "trading",
)

BFF = "bff"
CPP = "ores.http"


@dataclass(frozen=True)
class Rule:
    name: str
    match: tuple
    server: str | None
    kind: str
    area: str
    note: str


@dataclass(frozen=True)
class Exemption:
    name: str
    kind: str
    area: str
    note: str


RULES = (
    # A comment between rules.
    Rule("the books' route",
         ("regex", r"^(GET|PUT) /api/books(/\{id\})?$"),
         None, RESOURCE, "trading",
         "Books, read" ' and written.'),
    Rule("the storage capability", ("prefix", "GET {"), CPP, NOT_API, NO_AREA, ""),
    Rule("the blotter", ("exact", "GET /api/blotter"), BFF, COMPOSED, "trading",
         "One page of trades."),
    Rule(name="the accounts", match=("prefix", "GET /api/accounts"), server=None,
         kind=RESOURCE, area="access", note="Accounts."),
)


EXCEPTIONS = {
    (BFF, "GET", "/api/overview"): Exemption(
        "the deployment overview", COMPOSED, "access",
        "The administrator's home."),
}


VIEWS = {
    "the blotter": ("ores.trading", "the summaries", "ETag"),
    "the deployment overview": ("ores.iam", "the roster", "none"),
}
)f";

const std::string file1 = R"f(ACCESS = "/api/v1/access"

PAIRS = {
    ("GET", "/api/accounts"): ("GET", f"{ACCESS}/accounts"),
    ("GET", "/api/books"): ("GET", "/api/books"),
}

REFDATA_RECORDS = "projects/ores.web/packages/wire-protocol/src/records.ts:REFDATA_RECORDS"

FAMILIES = {
    ("GET", "/api/refdata/:resource"): (REFDATA_RECORDS, "GET", "/api/refdata/{resource}", "every"),
    ("PUT", "/api/refdata/:resource"): (REFDATA_RECORDS, "PUT", "/api/refdata/{resource}", "writable"),
}
)f";

const std::string file2 = R"f(export function register(server) {
    server.get('/api/books', async () => 1);
    server.put(
        "/api/books/{id}", async () => 1);
    app.get(`/api/blotter`, async () => 1);
    server.get('/api/overview', async () => 1);
    server.get('/api/accounts/:username', async () => 1);
    server.get('/api/refdata/:resource', async () => 1);
    server.put('/api/refdata/:resource', async () => 1);
    server.get('/api/unknown', async () => 1);
}
)f";

const std::string file3 = R"f(server.get('/api/tested', async () => 1);
)f";

const std::string file4 = R"f(void register_routes(router_t* router) {
    const std::string storage_prefix = "/api/storage";
    router->get("/api/books", handler);
    router->delete_("/api/books/{id}", handler);
    router->get(storage_prefix, handler);
    router->get(undeclared_name, handler);
}
)f";

const std::string file5 = R"f(router->get("/api/test-only", handler);
)f";

const std::string file6 = R"f(void r(router_t* router) {
    router->get("/api/v1/access/accounts", handler);
    router->get("/api/accounts", handler);
}
)f";

const std::string file7 = R"f(fetch('/api/blotter');
)f";

const std::string file8 = R"f(fetch('/api/overview');
)f";

const std::string file9 = R"f(export const REFDATA_RECORDS: Spec[] = [
    {
        key: 'currencies',
        keyFields: ['code'],
        writable: true,
    },
    {
        key: 'countries',
        keyFields: ['a', 'b'],
        writable: false,
    },
    ...[
        {
            key: 'rates',
        },
    ].map((r) => ({
        ...r,
        writes: Rate,
    })),
];
)f";

const std::string expected_tail = R"o(* The area map

The published areas (design section 11.2) and the areas this table
publishes beside them, each with the components that answer it today,
its size, and the kinds its routes are decided as. A route that is not
an API publishes no area, so it is counted under =(none)=.

| Area | Owner components | Routes | Kinds |
|------+------------------+--------+-------|
|  | access, books, trading | 5 | unclassified 5 |
| access | access, books | 3 | composed view 1, resource 2 |
| trading | books, trading | 4 | composed view 1, resource 3 |
| (none) | trading | 2 | not an API 2 |

* The composed views

A composed view owes four things, and the fourth is why the owner column
exists: without it the view is the old BFF again. The name is the view's
own; the schema is K1's to generate; the owner is the component that must
answer it after the port, which is not always the one that answers it
today.

Every route here is one a screen calls now, and the =Called by= column says
which client module calls it, so the port can prove a view with the same
request against either server. A view keeps its current route until the
cutover for that reason: moving it to the published namespace is a versioned
change of its own, not part of the port.

| View | Routes today | Called by | Owner | Computation | Cache policy |
|------+--------------+-----------+-------+-------------+--------------|
| the blotter | GET /api/blotter | app/src/blotter.ts | ores.trading | the summaries | ETag |
| the deployment overview | GET /api/overview | no client calls it | ores.iam | the roster | none |

* The coverage

How many BFF routes each area pairs with a C++ route, counted from
=build/scripts/route_pairing.py=. A pair records the mapping and does not
prove it; the port tasks prove each route against the answer the BFF gives.

| Area | BFF routes | Paired | Gaps |
|------+------------+--------+------|
| access | 2 | 0 | 2 |
| trading | 3 | 1 | 2 |
| (none) | 3 | 2 | 1 |
| total | 8 | 3 | 5 |

* The pairing

| BFF route | C++ equivalent |
|-----------+----------------|
| GET =/api/accounts/:username= | none |
| GET =/api/blotter= | none |
| GET =/api/books= | GET /api/books |
| PUT =/api/books/{id}= | none |
| GET =/api/overview= | none |
| GET =/api/refdata/:resource= | GET /api/refdata/{resource}, one per resource (3) |
| PUT =/api/refdata/:resource= | PUT /api/refdata/{resource}, one per resource (2) |
| GET =/api/unknown= | none |

* The inventory

14 routes: 8 answered by the BFF and 6 by ores.http.

| Kind | Area | Method | Path | Server | Where | Note |
|------+------+--------+------+--------+-------+------|
| resource | access | get | =/api/accounts= | ores.http | projects/ores.http/access/src/accounts.cpp:3 | Accounts. |
| resource | access | get | =/api/accounts/:username= | bff | projects/ores.web/packages/bff/src/books.ts:7 | Accounts. |
| composed view | trading | get | =/api/blotter= | bff | projects/ores.web/packages/bff/src/books.ts:5 | One page of trades. |
| resource | trading | get | =/api/books= | bff | projects/ores.web/packages/bff/src/books.ts:2 | Books, read and written. |
| resource | trading | get | =/api/books= | ores.http | projects/ores.http/trading/src/routes/books.cpp:3 | Books, read and written. |
|  |  | delete | =/api/books/{id}= | ores.http | projects/ores.http/trading/src/routes/books.cpp:4 |  |
| resource | trading | put | =/api/books/{id}= | bff | projects/ores.web/packages/bff/src/books.ts:3 | Books, read and written. |
| composed view | access | get | =/api/overview= | bff | projects/ores.web/packages/bff/src/books.ts:6 | The administrator's home. |
|  |  | get | =/api/refdata/:resource= | bff | projects/ores.web/packages/bff/src/books.ts:8 |  |
|  |  | put | =/api/refdata/:resource= | bff | projects/ores.web/packages/bff/src/books.ts:9 |  |
|  |  | get | =/api/unknown= | bff | projects/ores.web/packages/bff/src/books.ts:10 |  |
|  |  | get | =/api/v1/access/accounts= | ores.http | projects/ores.http/access/src/accounts.cpp:2 |  |
| not an API | (none) | get | ={storage_prefix}= | ores.http | projects/ores.http/trading/src/routes/books.cpp:5 | path composed from storage_prefix (declared at 2). |
| not an API | (none) | get | ={undeclared_name}= | ores.http | projects/ores.http/trading/src/routes/books.cpp:6 | path composed from undeclared_name. |
)o";


files tree() {
    return {{"build/scripts/route_inventory.py",
        "HEADER = r\"\"\"\\\n#+title: Fixture\n\n* How\n\"\"\"\n"},
        {"build/scripts/route_classification.py", file0},
        {"build/scripts/route_pairing.py", file1},
        {"projects/ores.web/packages/bff/src/books.ts", file2},
        {"projects/ores.web/packages/bff/src/books.test.ts", file3},
        {"projects/ores.http/trading/src/routes/books.cpp", file4},
        {"projects/ores.http/trading/tests/routes.cpp", file5},
        {"projects/ores.http/access/src/accounts.cpp", file6},
        {"projects/ores.web/packages/app/src/blotter.ts", file7},
        {"projects/ores.web/packages/app/src/overview.d.ts", file8},
        {"projects/ores.web/packages/wire-protocol/src/records.ts", file9}};
}

void replace(std::string& text, const std::string& from,
    const std::string& to) {
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
}

}

TEST_CASE("the inventory decides, pairs and counts every registered route "
    "as the script writes it", "[codecs][route_inventory]") {
    const auto out = tests::generate(codecs::route_inventory, tree());
    REQUIRE(out.size() == 1);
    const auto& text = out.at(inventory_path);
    CHECK(text.starts_with("\\\n#+title: Fixture\n\n* How\n\n* The area map\n"));
    CHECK(text.ends_with(expected_tail));
}

TEST_CASE("an edited rule in the classification module is seen",
    "[codecs][route_inventory]") {
    auto t = tree();
    replace(t.at("build/scripts/route_classification.py"),
        "\"Books, read\" ' and written.'", "\"Books, planted.\"");
    const auto out = tests::generate(codecs::route_inventory, t);
    CHECK(out.at(inventory_path).find(
        "| resource | trading | get | =/api/books= | bff | "
        "projects/ores.web/packages/bff/src/books.ts:2 | Books, planted. |") !=
        std::string::npos);
}

TEST_CASE("a pair module the reader cannot read writes nothing",
    "[codecs][route_inventory]") {
    auto t = tree();
    replace(t.at("build/scripts/route_pairing.py"), "f\"{ACCESS}/accounts\"",
        "f\"{MISSING}/accounts\"");
    CHECK(tests::generate(codecs::route_inventory, t).empty());
}

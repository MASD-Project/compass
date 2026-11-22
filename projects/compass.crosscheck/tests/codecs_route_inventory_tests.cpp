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

#include <algorithm>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.codec/ops/data_tables.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;
using Catch::Matchers::ContainsSubstring;

const std::string inventory_path =
    "doc/knowledge/architecture/http_route_inventory.org";

namespace cops = compass::codec::ops;
using rows = std::vector<std::vector<std::string>>;

const std::string document = "compass/checks/route_inventory.org";

cops::data_table table(std::string name, std::uint32_t line,
    std::vector<std::string> columns, rows cells) {
    cops::data_table t{.name = std::move(name), .line = line,
        .columns = std::move(columns), .rows = std::move(cells),
        .row_lines = {}};
    for (std::size_t r = 0; r < t.rows.size(); ++r)
        t.row_lines.push_back(line + 2 + static_cast<std::uint32_t>(r));
    return t;
}

/**
 * @brief The check document of a made-up project: its head, settings,
 * rules, exemptions, views, pairs and families. route_inventory.py
 * --write was run on the same decisions over this fixture; the expected
 * text is what it wrote after its header.
 */
cops::data_tables check_document() {
    cops::data_tables t{.source = document, .tables = {}};
    t.tables = {
        table("Head", 10, {"line"},
            {{"\\"}, {"#+title: Fixture"}, {""}, {"* How"}}),
        table("Settings", 20, {"setting", "value"},
            {{"composed kind", "composed view"}, {"no area", "(none)"}}),
        table("Rules", 30, {"name", "match", "pattern", "server", "kind",
            "area", "note"},
            {{"the books' route", "regex", R"(^(GET|PUT) /api/books(/\{id\})?$)",
              "", "resource", "trading", "Books, read and written."},
             {"the storage capability", "prefix", "GET {", "ores.http",
              "not an API", "(none)", ""},
             {"the blotter", "exact", "GET /api/blotter", "bff",
              "composed view", "trading", "One page of trades."},
             {"the accounts", "prefix", "GET /api/accounts", "", "resource",
              "access", "Accounts."}}),
        table("Exemptions", 40, {"server", "method", "path", "name", "kind",
            "area", "note"},
            {{"bff", "GET", "/api/overview", "the deployment overview",
              "composed view", "access", "The administrator's home."}}),
        table("Views", 50, {"view", "owner", "computation", "cache policy"},
            {{"the blotter", "ores.trading", "the summaries", "ETag"},
             {"the deployment overview", "ores.iam", "the roster", "none"}}),
        table("Pairs", 60, {"BFF method", "BFF path", "C++ method",
            "C++ path"},
            {{"GET", "/api/accounts", "GET", "/api/v1/access/accounts"},
             {"GET", "/api/books", "GET", "/api/books"}}),
        table("Families", 70, {"BFF method", "BFF path", "registry", "array",
            "C++ method", "C++ path", "needs"},
            {{"GET", "/api/refdata/:resource",
              "projects/ores.web/packages/wire-protocol/src/records.ts",
              "REFDATA_RECORDS", "GET", "/api/refdata/{resource}", "every"},
             {"PUT", "/api/refdata/:resource",
              "projects/ores.web/packages/wire-protocol/src/records.ts",
              "REFDATA_RECORDS", "PUT", "/api/refdata/{resource}",
              "writable"}})};
    return t;
}

tests::options data(const cops::data_tables& t) {
    return {{std::string(cops::data_option), cops::encode_data_tables(t)}};
}

cops::data_table& in(cops::data_tables& t, std::string_view name) {
    for (auto& table : t.tables)
        if (table.name == name)
            return table;
    FAIL("no table " << name);
    return t.tables.front();
}

/**
 * @brief The messages of what one run over @p t with @p doc found.
 */
std::vector<std::string> messages(const files& t,
    const cops::data_tables& doc) {
    std::vector<std::string> out;
    for (const auto& f : tests::run(codecs::route_inventory, t,
             data(doc)).findings)
        out.push_back(std::get<2>(f));
    return out;
}

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
    return {{"projects/ores.web/packages/bff/src/books.ts", file2},
        {"projects/ores.web/packages/bff/src/books.test.ts", file3},
        {"projects/ores.http/trading/src/routes/books.cpp", file4},
        {"projects/ores.http/trading/tests/routes.cpp", file5},
        {"projects/ores.http/access/src/accounts.cpp", file6},
        {"projects/ores.web/packages/app/src/blotter.ts", file7},
        {"projects/ores.web/packages/app/src/overview.d.ts", file8},
        {"projects/ores.web/packages/wire-protocol/src/records.ts", file9}};
}

}

TEST_CASE("the inventory decides, pairs and counts every registered route "
    "from the tables of the check document", "[codecs][route_inventory]") {
    const auto out = tests::generate(codecs::route_inventory, tree(),
        data(check_document()));
    REQUIRE(out.size() == 1);
    const auto& text = out.at(inventory_path);
    CHECK(text.starts_with("\\\n#+title: Fixture\n\n* How\n\n* The area map\n"));
    CHECK(text.ends_with(expected_tail));
}

TEST_CASE("a checkout without the route scripts writes the inventory and "
    "reports nothing", "[codecs][route_inventory]") {
    const auto t = tree();
    REQUIRE(std::ranges::none_of(t, [](const auto& f) {
        return f.first.starts_with("build/scripts/");
    }));
    CHECK(tests::run(codecs::route_inventory, t,
        data(check_document())).findings.empty());
    CHECK(tests::generate(codecs::route_inventory, t,
        data(check_document())).size() == 1);
}

TEST_CASE("an edited row of the check document is seen",
    "[codecs][route_inventory]") {
    auto doc = check_document();
    in(doc, "Rules").rows[0][6] = "Books, planted.";
    in(doc, "Pairs").rows[1][3] = "/api/v2/books";
    const auto out = tests::generate(codecs::route_inventory, tree(),
        data(doc));
    const auto& text = out.at(inventory_path);
    CHECK(text.find(
        "| resource | trading | get | =/api/books= | bff | "
        "projects/ores.web/packages/bff/src/books.ts:2 | Books, planted. |") !=
        std::string::npos);
    CHECK(text.find("| GET =/api/books= | GET /api/v2/books |") !=
        std::string::npos);
}

TEST_CASE("a check document missing a table or a column is unreadable and "
    "writes nothing", "[codecs][route_inventory]") {
    auto no_table = check_document();
    std::erase_if(no_table.tables, [](const cops::data_table& t) {
        return t.name == "Families";
    });
    CHECK(tests::generate(codecs::route_inventory, tree(),
        data(no_table)).empty());
    auto found = messages(tree(), no_table);
    REQUIRE(found.size() == 1);
    CHECK_THAT(found.front(), ContainsSubstring("compass/checks/"
        "route_inventory.org has no table under the level-1 heading "
        "\"* Families\", which route_inventory reads"));

    auto no_column = check_document();
    in(no_column, "Views").columns[3] = "cache";
    CHECK(tests::generate(codecs::route_inventory, tree(),
        data(no_column)).empty());
    found = messages(tree(), no_column);
    REQUIRE(found.size() == 1);
    CHECK_THAT(found.front(), ContainsSubstring("the table \"Views\" has no "
        "column cache policy, which route_inventory reads"));

    const auto none = tests::run(codecs::route_inventory, tree()).findings;
    REQUIRE(none.size() == 1);
    CHECK_THAT(std::get<2>(none.front()), ContainsSubstring("write compass/checks/"
        "route_inventory.org with :CODEC: route_inventory"));
}

TEST_CASE("a row of the check document that does not read is unreadable "
    "and names its line", "[codecs][route_inventory]") {
    const auto unreadable = [](auto edit, std::string_view why) {
        auto doc = check_document();
        edit(doc);
        CHECK(tests::generate(codecs::route_inventory, tree(),
            data(doc)).empty());
        const auto found = messages(tree(), doc);
        REQUIRE(found.size() == 1);
        CHECK_THAT(found.front(), ContainsSubstring(std::string(why)));
    };
    unreadable([](cops::data_tables& d) {
        in(d, "Rules").rows[1][1] = "glob";
    }, "compass/checks/route_inventory.org:33: the rule 'the storage "
       "capability' matches by 'glob', not exact, prefix or regex, in the "
       "table \"Rules\"");
    unreadable([](cops::data_tables& d) {
        in(d, "Rules").rows[0][2] = "^(GET";
    }, "the pattern of the rule 'the books' route' does not compile");
    unreadable([](cops::data_tables& d) {
        in(d, "Families").rows[1][6] = "some";
    }, "the family PUT /api/refdata/:resource needs 'some'");
    unreadable([](cops::data_tables& d) {
        auto& pairs = in(d, "Pairs");
        pairs.rows.push_back(pairs.rows[0]);
        pairs.row_lines.push_back(64);
    }, ":64: the BFF route GET /api/accounts is paired twice");
    unreadable([](cops::data_tables& d) {
        auto& settings = in(d, "Settings");
        settings.rows.pop_back();
        settings.row_lines.pop_back();
    }, "no row states the setting 'no area'");
}

TEST_CASE("a family whose registry the checkout lacks is refused",
    "[codecs][route_inventory]") {
    auto t = tree();
    t.erase("projects/ores.web/packages/wire-protocol/src/records.ts");
    CHECK(tests::generate(codecs::route_inventory, t,
        data(check_document())).empty());
    const auto found = messages(t, check_document());
    REQUIRE(found.size() == 1);
    CHECK_THAT(found.front(), ContainsSubstring("cannot read projects/"
        "ores.web/packages/wire-protocol/src/records.ts"));
}

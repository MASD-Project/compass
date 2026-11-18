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
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.crosscheck/codecs/model_rules.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

std::vector<std::string> messages(const tests::outcome& r) {
    std::vector<std::string> out;
    for (const auto& [path, line, message] : r.findings)
        out.push_back(path + ":" + std::to_string(line) + ": " + message);
    return out;
}

std::string column(std::string_view name, std::string_view drawer) {
    return "** " + std::string(name) + "\n:PROPERTIES:\n" +
        std::string(drawer) + ":END:\n";
}

const std::string swap = "projects/ores.trading/modeling/ores.trading.swap.org";

}

TEST_CASE("a money column typed inexactly and a continuous one typed exact "
    "are reported on their headings", "[codecs][model_rules]") {
    const files t{
        {swap,
         "#+title: swap\n#+type: ores.codegen.entity\n\n* Columns\n" +
         column("notional_amount", ":type: double precision\n"
             ":cpp_type: double\n") +
         column("strike", ":type: numeric(38,12)\n:cpp_type: decimal\n") +
         column("face_value", ":type: text\n:cpp_type: std::string\n") +
         column("recovery_rate", ":type: numeric\n") +
         column("premium_date", ":type: date\n") +
         column("price", ":type: NUMERIC(38, 12)\n"
             ":cpp_type: std::optional<double>\n")},
        {"projects/ores.trading/core/modeling/ores.trading.fee_field_group.org",
         "#+type: ores.codegen.field_group\n* Fields\n" +
         column("fee", ":cpp_type: std::string\n") +
         column("weight", ":cpp_type: decimal\n")},
        {"projects/ores.trading/modeling/ores.trading.fx_variance_swap_instrument.org",
         "#+type: ores.codegen.entity\n* Columns\n" +
         column("strike", ":type: numeric\n")},
        {"projects/ores.synthetic/modeling/ores.synthetic.ir_curve_generation_config.org",
         "#+type: ores.codegen.entity\n* Columns\n" +
         column("initial_rate", ":type: double precision\n")}};
    CHECK(messages(tests::run(codecs::model_rules, t)) ==
        std::vector<std::string>{
        "projects/ores.trading/core/modeling/ores.trading.fee_field_group.org:3: "
        "fee is money, typed sql=(embedded) cpp=std::string",
        "projects/ores.trading/core/modeling/ores.trading.fee_field_group.org:7: "
        "weight is a continuous quantity, typed sql=(embedded) cpp=decimal",
        "projects/ores.trading/modeling/ores.trading.fx_variance_swap_instrument.org:3: "
        "strike is a continuous quantity, typed sql=numeric cpp=-",
        "projects/ores.trading/modeling/ores.trading.swap.org:5: "
        "notional_amount is money, typed sql=double precision cpp=double",
        "projects/ores.trading/modeling/ores.trading.swap.org:15: "
        "face_value is money, typed sql=text cpp=std::string",
        "projects/ores.trading/modeling/ores.trading.swap.org:20: "
        "recovery_rate is a continuous quantity, typed sql=numeric cpp=-",
        "projects/ores.trading/modeling/ores.trading.swap.org:28: "
        "price is money, typed sql=NUMERIC(38, 12) "
        "cpp=std::optional<double>"});
}

TEST_CASE("only the columns the loader reads are money columns",
    "[codecs][model_rules]") {
    const auto drawer = ":type: double precision\n";
    const files t{
        {swap,
         "#+type: ores.codegen.entity\n* Notes\n" + column("price", drawer) +
         "#+begin_src org\n** amount\n:type: real\n#+end_src\n"
         "* Columns\n" + column("trade_date", ":type: date\n")},
        {"projects/modeling/swap_view.org",
         "#+type: entity_view\n* Columns\n" + column("price", drawer)},
        {"projects/ores.trading/modeling/notes.org",
         "#+type: knowledge\n* Columns\n" + column("price", drawer)}};
    CHECK(tests::run(codecs::model_rules, t).findings.empty());
}

TEST_CASE("an edit to one model reports what a run from scratch reports",
    "[codecs][model_rules]") {
    const files before{
        {swap, "#+type: ores.codegen.entity\n* Columns\n" +
             column("notional", ":type: numeric\n")},
        {"projects/ores.trading/modeling/ores.trading.leg.org",
         "#+type: ores.codegen.entity\n* Columns\n" +
         column("spread", ":type: real\n")}};
    auto after = before;
    after[swap] = "#+type: ores.codegen.entity\n* Columns\n" +
        column("notional", ":type: real\n");
    const auto r = tests::run_after_edit(codecs::model_rules, before, after);
    CHECK(r.findings.size() == 2);
}

TEST_CASE("an HTTP area outside the published ones is reported",
    "[codecs][model_rules]") {
    const std::string module = "build/scripts/route_classification.py";
    const files t{
        {module,
         "NO_AREA = \"(none)\"\n\nAREAS = (\n    \"access\",  # people\n"
         "    \"trading\",\n    'market-data',\n    NO_AREA_SPELLED := 1,\n)\n"},
        {"projects/ores.a/modeling/a.org",
         "#+type: ores.codegen.entity\n#+http_area: trading\n"}};
    CHECK(tests::messages_of(tests::run(codecs::model_rules, t)) ==
        std::vector<std::string>{"AREAS or NO_AREA is not a literal this "
            "check can read"});
    const files u{
        {module,
         "NO_AREA = \"(none)\"\n\nAREAS = (\n    \"access\",  # people\n"
         "    \"(none)\",\n    'market-data',\n)\n"},
        {"projects/ores.a/modeling/a.org",
         "#+type: ores.codegen.entity\n#+http_area: access\n"
         "#+http_area: (none)\n#+http_area: gone \n"
         "#+begin_src org\n#+http_area: in_a_block\n#+end_src\n"},
        {"projects/ores.a/modeling/b.org",
         "#+type: knowledge\n#+http_area: gone\n"},
        {"projects/ores.a/modeling/deep/c.org",
         "#+type: ores.codegen.entity\n#+http_area: gone\n"}};
    CHECK(tests::places(tests::run(codecs::model_rules, u).findings) ==
        std::vector<std::pair<std::string, std::uint32_t>>{
            {"projects/ores.a/modeling/a.org", 3},
            {"projects/ores.a/modeling/a.org", 4}});
}

TEST_CASE("a model may not contradict the profile it binds to",
    "[codecs][model_rules]") {
    const auto drawer = [](std::string_view body) {
        return ":PROPERTIES:\n" + std::string(body) + ":END:\n";
    };
    const files t{
        {"projects/modeling/variability_uuid_lookup.org",
         "#+type: profile\n* Assignments\n| Feature | Value |\n|---+---|\n"
         "| [[id:X][has_tenant_id]] | true |\n| has_pagination | false |\n"
         "| parent_entity_singular | required |\n| read_only | --- |\n"
         "* Notes\n| other | x |\n"},
        {"projects/modeling/variability_paged.org",
         "#+type: profile\n* Assignments\n| has_pagination | true |\n"},
        {"projects/ores.a/modeling/ores.a.good.org",
         "#+type: ores.codegen.entity\n* Flags\n" +
         drawer(":profile: uuid-lookup\n:has_tenant_id: true\n"
             ":parent_entity_singular: book\n:read_only: true\n") +
         "#+begin_src org\n:has_tenant_id: false\n#+end_src\n"},
        {"projects/ores.a/modeling/ores.a.bad.org",
         "#+type: ores.codegen.entity\n* Flags\n" +
         drawer(":profile: uuid-lookup, paged, nope\n:has_tenant_id: false\n") +
         "* C++\n** Presentation\n" + drawer(":has_pagination: true\n") +
         "** Other\n" + drawer(":has_pagination: false\n")}};
    CHECK(tests::messages_of(tests::run(codecs::model_rules, t)) ==
        std::vector<std::string>{
            "profile 'uuid-lookup' fixes has_tenant_id=true, model declares "
            "false",
            "states has_pagination twice with different values (C++ / "
            "Presentation=true; C++ / Other=false)",
            "profile 'uuid-lookup' requires parent_entity_singular to be "
            "set, model states none"});
}

TEST_CASE("an edit to a profile reruns the rules of the models that bind it",
    "[codecs][model_rules]") {
    const std::string profile = "projects/modeling/variability_paged.org";
    const files before{
        {profile, "#+type: profile\n* Assignments\n| has_pagination | true |\n"},
        {"projects/ores.a/modeling/ores.a.m.org",
         "#+type: ores.codegen.entity\n* Flags\n:PROPERTIES:\n"
         ":profile: paged\n:has_pagination: false\n:END:\n"}};
    auto after = before;
    after[profile] = "#+type: profile\n* Assignments\n"
        "| has_pagination | false |\n";
    CHECK(tests::run(codecs::model_rules, before).findings.size() == 1);
    CHECK(tests::run_after_edit(codecs::model_rules, before, after)
        .findings.empty());
}

TEST_CASE("a declared subject outside the protocol is reported on its line",
    "[codecs][model_rules]") {
    const files t{
        {"projects/ores.refdata/modeling/ores.refdata.currency.org",
         ":PROPERTIES:\n"
         ":subject: refdata.v1.currencies.get\n"
         ":subject: refdata.v1.currencies.list_by_party\n"
         ":subject: refdata.v1.events.currencies.created\n"
         ":subject: refdata.v1.ops.rebuild\n"
         ":subject: {component}.v1.history.get\n"
         ":subject: refdata.v1.currencies.fetch\n"
         ":subject: refdata.v2.currencies.get\n"
         ":subject: refdata.v1.currency_events.updated\n"
         ":subject: refdata.v1.events.currencies.renamed\n"
         ":subject: refdata.v1.Currencies.get\n"
         ":subject: refdata.currencies.get\n"
         ":END:\n#+type: ores.codegen.operation\n"},
        {"projects/ores.refdata/modeling/nested/ores.refdata.x.org",
         "#+type: ores.codegen.operation\n* M\n:PROPERTIES:\n"
         ":subject: refdata.v1.nested.fetch\n:END:\n"},
        {"projects/ores.refdata/modeling/ores.refdata.y.org",
         "#+type: ores.codegen.operation\n#+begin_src org\n"
         ":subject: refdata.v1.in_a_block.fetch\n#+end_src\n"}};
    CHECK(messages(tests::run(codecs::model_rules, t)) ==
        std::vector<std::string>{
            "projects/ores.refdata/modeling/ores.refdata.currency.org:7: "
            "refdata.v1.currencies.fetch: the verb 'fetch' is not one of the "
            "closed set of eight and is not in the reserved ops namespace",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:8: "
            "refdata.v2.currencies.get: the version segment is 'v2', not "
            "'v1'",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:9: "
            "refdata.v1.currency_events.updated: the four-segment event "
            "spelling; an event names the events namespace, as in "
            "component.v1.events.collection.action",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:10: "
            "refdata.v1.events.currencies.renamed: the event action "
            "'renamed' is not one of created, updated, deleted",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:11: "
            "refdata.v1.Currencies.get: the resource segment 'Currencies' is "
            "not lower snake_case",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:12: "
            "refdata.currencies.get: 3 segments, the grammar has four, or "
            "five for an event"});
}

TEST_CASE("a drawer that switches the TypeScript protocol facet is an "
    "opt-out", "[codecs][model_rules]") {
    const files t{
        {"projects/ores.refdata/modeling/ores.refdata.x.org",
         ":PROPERTIES:\n  :ores.ts.protocol.enabled: false\n:END:\n"
         "#+type: ores.codegen.entity\n"
         "The key :ores.ts.protocol.enabled: in prose is not a switch.\n"},
        {"projects/ores.refdata/modeling/notes.org",
         ":PROPERTIES:\n:ores.ts.protocol.enabled: false\n:END:\n"
         "#+type: knowledge\n"}};
    CHECK(messages(tests::run(codecs::model_rules, t)) ==
        std::vector<std::string>{
            "projects/ores.refdata/modeling/ores.refdata.x.org:2: the model "
            "opts out of the TypeScript protocol facet: "
            ":ores.ts.protocol.enabled: false"});
}

namespace {

std::string routed(std::string_view singular, std::string_view enabled,
    std::string_view type = "ores.codegen.entity") {
    return ":PROPERTIES:\n:ores.cpp.http-route.enabled: " +
        std::string(enabled) + "\n:END:\n#+type: " + std::string(type) +
        "\n" + std::string(singular) + "\n";
}

const std::string aggregate =
    "projects/ores.http/refdata/src/routes/refdata/refdata_routes.cpp";

}

TEST_CASE("a route unit a model opts into is registered once",
    "[codecs][model_rules]") {
    const files t{
        {aggregate,
         "void register_all() {\n    currency_routes::register_routes(r);\n"
         "    currency_routes::register_routes(r);\n"
         "    ghost_routes::register_routes(r);\n"
         "    // party_routes::register_routes(r);\n}\n"},
        {"projects/ores.http/refdata/src/routes/refdata/currency_routes.cpp",
         ""},
        {"projects/ores.http/iam/src/other.cpp", ""},
        {"projects/ores.refdata/modeling/ores.refdata.currency.org",
         routed("#+entity_singular: currency", "true")},
        {"projects/ores.refdata/modeling/ores.refdata.party.org",
         routed("#+name_singular: party", "yes")},
        {"projects/ores.refdata/modeling/ores.refdata.book.org",
         routed("#+entity_singular: book", "false")},
        {"projects/ores.refdata/modeling/ores.refdata.desk.org",
         "#+type: ores.codegen.entity\n#+entity_singular: desk\n* Flags\n"
         ":PROPERTIES:\n:ores.cpp.http-route.enabled: true\n:END:\n"},
        {"projects/ores.refdata/modeling/notes.org",
         routed("#+entity_singular: note", "true", "knowledge")},
        {"projects/ores.iam/modeling/ores.iam.account.org",
         routed("#+entity_singular: account", "true")}};
    CHECK(messages(tests::run(codecs::model_rules, t)) ==
        std::vector<std::string>{
            aggregate + ":1: never registers party_routes, which "
            "projects/ores.refdata/modeling/ores.refdata.party.org opts into; "
            "the gateway answers nothing for it and nothing reports it",
            aggregate + ":1: registers currency_routes 2 times; the second "
            "registration claims routes the first stated",
            aggregate + ":1: registers ghost_routes, which has no unit in "
            "this component"});
}

TEST_CASE("an edit to a model's route opt-in reruns the aggregate's rules",
    "[codecs][model_rules]") {
    const auto book = "projects/ores.refdata/modeling/ores.refdata.book.org";
    const files before{
        {aggregate, "    party_routes::register_routes(r);\n"},
        {"projects/ores.http/refdata/src/routes/refdata/party_routes.cpp", ""},
        {"projects/ores.refdata/modeling/ores.refdata.party.org",
         routed("#+entity_singular: party", "true")},
        {book, routed("#+entity_singular: book", "false")}};
    CHECK(tests::run(codecs::model_rules, before).findings.empty());
    auto after = before;
    after[book] = routed("#+entity_singular: book", "true");
    CHECK(tests::messages_of(tests::run_after_edit(codecs::model_rules,
        before, after)) == std::vector<std::string>{"never registers "
            "book_routes, which projects/ores.refdata/modeling/"
            "ores.refdata.book.org opts into; the gateway answers nothing "
            "for it and nothing reports it"});
}

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
#include "compass.crosscheck/codecs/marketdata_identity_columns.hpp"

#include <catch2/catch_test_macros.hpp>
#include "entity_model.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

const std::string schema =
    "enum class field {\n    tenor,\n    currency,\n    strike,\n};\n"
    "using f = field;\n"
    "inline constexpr std::array fx_rows{id(f::currency), at(f::tenor), "
    "at_or_none(f::strike)};\n"
    "inline constexpr std::array rates_rows{id(f::currency), "
    "id_or_none(f::tenor)};\n"
    "{detail::t::fx_spot, detail::fx_rows, asset_class::fx, field::rate}\n"
    "{detail::t::swap, detail::rates_rows, asset_class::rates, "
    "field::rate}\n";

const std::string grammar =
    "inline constexpr std::array<index_field_spec, 1> ibor_fields{"
    "{\"tenor_label\", true}};\n"
    "{index_family::ibor, asset_class::rates, \"index_name\", "
    "detail::ibor_fields}\n";

const std::string projector =
    "void assign_index(row& row, std::string_view name, std::string text) {\n"
    "    if (name == \"index_name\")\n        row.index_name = text;\n"
    "    if (name == \"tenor_label\")\n        row.tenor_label = text;\n}\n"
    "void place(row& row, field f, std::string text) {\n"
    "    switch (f) {\n"
    "    case field::currency:\n        row.currency = text;\n        break;\n"
    "    case field::tenor:\n        row.tenor = text;\n        break;\n"
    "    case field::strike:\n        break;\n"
    "    }\n}\n";

std::string text_column(std::string_view name) {
    return tests::model_column(name, ":type: text\n:cpp_type: std::string\n"
        ":nullable: true\n");
}

const std::string table = tests::entity_model("marketdata",
    "market_series_identity", "ores_marketdata_market_series_identity_tbl",
    tests::model_column("series_id", ":type: uuid\n"
        ":cpp_type: boost::uuids::uuid\n:primary_key: true\n") +
    tests::model_column("party_id", ":type: uuid\n"
        ":cpp_type: boost::uuids::uuid\n") +
    text_column("identity_kind") + text_column("asset_class") +
    text_column("instrument_type") + text_column("quote_type") +
    text_column("currency") + text_column("tenor") +
    text_column("index_name") + text_column("tenor_label"), {},
    ":has_tenant_id: true\n", {},
    ":current_state: true\n:no_audit_columns: true\n");

std::string view(std::string_view asset, std::string_view columns,
    std::string_view types) {
    return "create or replace view ores_marketdata_series_identity_" +
        std::string(asset) + "_vw\n    with (security_invoker = true) as\n"
        "select series_id, tenant_id, party_id, instrument_type, quote_type, "
        "asset_class, identity_kind" + std::string(columns) + "\n"
        "from ores_marketdata_market_series_identity_tbl\n"
        "where identity_kind = 'series'\n"
        "and instrument_type in (" + std::string(types) + ");\n";
}

files tree(std::string s, std::string p, std::string t, std::string views) {
    return {
        {"projects/ores.marketdata/api/include/ores.marketdata.api/datum/"
         "schema.hpp", s},
        {"projects/ores.marketdata/api/include/ores.marketdata.api/datum/"
         "market_index.hpp", grammar},
        {"projects/ores.marketdata/core/src/repository/"
         "market_series_identity_projector.cpp", p},
        {"projects/ores.marketdata/modeling/"
         "ores.marketdata.market_series_identity.org", t},
        {"projects/ores.sql/create/marketdata/"
         "marketdata_series_identity_views_create.sql", views}};
}

std::string replaced(std::string text, std::string_view from,
    std::string_view to) {
    text.replace(text.find(from), from.size(), to);
    return text;
}

}

TEST_CASE("a projection in step with the schema and the grammar passes",
    "[codecs][marketdata_identity_columns]") {
    const auto t = tree(schema, projector, table,
        view("fx", ", currency", "'fx_spot'") +
        view("rates", ", currency, \"tenor\"", " 'swap' "));
    CHECK(tests::run(codecs::marketdata_identity_columns, t).findings
        .empty());
}

TEST_CASE("each disagreement of the projection is reported",
    "[codecs][marketdata_identity_columns]") {
    const auto t = tree(schema,
        replaced(projector, "    case field::strike:\n",
            "    case field::strike:\n        row.strike = text;\n"),
        replaced(table, "** tenor_label\n", text_column("legacy") +
            "** tenor_label\n"),
        view("fx", ", currency AS kind", "'fx_spot', 'fx_fwd'") +
        view("rates", ", currency", "'swap'"));
    CHECK(tests::messages_of(tests::run(codecs::marketdata_identity_columns,
            t)) == std::vector<std::string>{
        "ores_marketdata_market_series_identity_tbl has column 'legacy', "
        "which is neither a context column, nor an identity field, nor an "
        "index field.",
        "the projector places coordinate field 'strike' in column 'strike'; "
        "a series holds no coordinate.",
        "the fx view exposes column 'kind', which is not one of its identity "
        "fields.",
        "the fx view does not expose identity field 'currency'.",
        "the fx view names instrument type 'fx_fwd', which does not belong "
        "to fx.",
        "the rates view does not expose identity field 'tenor'."});
}

TEST_CASE("a source that lost the shape the check reads is one finding",
    "[codecs][marketdata_identity_columns]") {
    const auto t = tree(replaced(schema, "using f = field;", ""), projector,
        table, view("fx", "", "'fx_spot'"));
    CHECK(tests::messages_of(tests::run(codecs::marketdata_identity_columns,
            t)) == std::vector<std::string>{"does not hold 'using f = "
        "field;', which the check reads through"});
}

TEST_CASE("the table's columns are those its model gives the create script",
    "[codecs][marketdata_identity_columns]") {
    const auto views = view("fx", ", currency", "'fx_spot'") +
        view("rates", ", currency, \"tenor\"", " 'swap' ");
    const auto temporal = replaced(table, ":current_state: true\n", "");
    CHECK(tests::messages_of(tests::run(codecs::marketdata_identity_columns,
            tree(schema, projector, temporal, views))) ==
        std::vector<std::string>{
            "ores_marketdata_market_series_identity_tbl has column "
            "'valid_from', which is neither a context column, nor an "
            "identity field, nor an index field.",
            "ores_marketdata_market_series_identity_tbl has column "
            "'valid_to', which is neither a context column, nor an "
            "identity field, nor an index field."});
    const auto elsewhere = replaced(table,
        "ores_marketdata_market_series_identity_tbl", "ores_other_tbl");
    CHECK(tests::messages_of(tests::run(codecs::marketdata_identity_columns,
            tree(schema, projector, elsewhere, views))) ==
        std::vector<std::string>{"does not load as the model whose create "
            "script makes ores_marketdata_market_series_identity_tbl"});
}

TEST_CASE("the context and view columns are the project's to state",
    "[codecs][marketdata_identity_columns]") {
    const auto views = view("fx", ", currency", "'fx_spot'") +
        view("rates", ", currency, \"tenor\"", " 'swap' ");
    const auto provided = replaced(table, "** tenor_label\n",
        text_column("provider_code") + "** tenor_label\n");
    const auto t = tree(schema, projector, provided, views);
    CHECK(tests::messages_of(tests::run(codecs::marketdata_identity_columns,
            t)) == std::vector<std::string>{
        "ores_marketdata_market_series_identity_tbl has column "
        "'provider_code', which is neither a context column, nor an "
        "identity field, nor an index field."});
    CHECK(tests::run(codecs::marketdata_identity_columns, t,
        {{"context_columns", "series_id, tenant_id,party_id,identity_kind,"
            "asset_class,instrument_type,quote_type,provider_code"}})
        .findings.empty());

    const auto narrowed = tests::run(codecs::marketdata_identity_columns,
        tree(schema, projector, table, views),
        {{"view_columns", "series_id,tenant_id,party_id,instrument_type,"
            "quote_type,asset_class"}});
    CHECK(tests::messages_of(narrowed) == std::vector<std::string>{
        "the fx view exposes column 'identity_kind', which is not one of its "
        "identity fields.",
        "the rates view exposes column 'identity_kind', which is not one of "
        "its identity fields."});
}

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

const std::string table =
    "create table if not exists \"ores_marketdata_market_series_identity_tbl\" (\n"
    "    \"series_id\" uuid not null,\n    \"tenant_id\" uuid,\n"
    "    \"party_id\" uuid,\n    \"identity_kind\" text,\n"
    "    \"asset_class\" text,\n    \"instrument_type\" text,\n"
    "    \"quote_type\" text,\n    \"currency\" text,\n    \"tenor\" text,\n"
    "    \"index_name\" text,\n    \"tenor_label\" text,\n"
    "    primary key (\"series_id\")\n);\n";

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
        {"projects/ores.sql/create/marketdata/"
         "marketdata_market_series_identity_create.sql", t},
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
        replaced(table, "    \"tenor_label\" text,\n",
            "    \"tenor_label\" text,\n    \"legacy\" text,\n"),
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

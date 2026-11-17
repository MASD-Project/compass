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
#include "compass.crosscheck/codecs/script_library_seed.hpp"

#include <array>
#include <map>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass;
namespace ct = compass::codec::types;
using files = std::map<std::string, std::string, std::less<>>;

const std::string library =
    "external/ore/examples/Products/Input/scriptlibrary.xml";
const std::string seed =
    "projects/ores.sql/populate/trading/trading_script_library_populate.sql";

files generate(const files& all) {
    ct::checkout_files checkout;
    for (const auto& [path, bytes] : all)
        checkout.paths.push_back(path);
    checkout.content_of = [&](std::string_view p) {
        return std::optional(store::ops::blob_id_of(all.find(p)->second));
    };
    checkout.read = [&](std::string_view p) {
        return std::optional(all.find(p)->second);
    };
    ct::corpus corpus;
    corpus.files = &checkout;
    const org::types::type_registry types;
    ct::configuration configuration;
    configuration.set("script_library_seed", "enabled", "true");
    const std::array codecs{crosscheck::codecs::script_library_seed};
    const auto r = codec::ops::run(codec::ops::make_registry(codecs, types,
        configuration), corpus, types, 1);
    files out;
    const auto& f = r.files[0];
    for (std::uint32_t i = 0; i < f.size(); ++i)
        out.emplace(f.paths[ct::file_id{i}], f.contents[ct::file_id{i}]);
    return out;
}

/**
 * @brief The rows of @p table in @p text, each up to its audit columns.
 */
std::string rows_of(const std::string& text, const std::string& table) {
    const auto at = text.find("insert into " + table + " (");
    REQUIRE(at != std::string::npos);
    const auto begin = text.find(") values\n", at) + 9;
    std::string out;
    std::size_t line = begin;
    while (text.compare(line, 4, "    ") == 0) {
        const auto end = text.find(", current_user", line);
        out += text.substr(line + 4, end - line - 4) + "\n";
        line = text.find('\n', end) + 1;
    }
    return out;
}

}

TEST_CASE("a script's inputs are the names its variants read, typed by the "
    "calls that read them, with their rules and comment text",
    "[codecs][script_library_seed]") {
    const auto out = generate({{library, R"(<ScriptLibrary>
  <!-- Swing
       Notional    the amount -->
  <Script>
    <Name> Swing </Name>
    <Script purpose="aux">
      <Code>Extra = Spread;</Code>
    </Script>
    <Script>
      <Code><![CDATA[NUMBER i; REQUIRE  Notional >   0;
        Value = PAY(Notional * Under(Dates[i]), Dates[i], Pay, Ccy);]]></Code>
      <NPV> Value </NPV>
    </Script>
  </Script>
</ScriptLibrary>
)"}});

    const auto& text = out.at(seed);
    CHECK(rows_of(text, "ores_trading_scripts_tbl") ==
        "('Swing', ores_utility_system_tenant_id_fn(), 0, null, 'Swing'\n");
    // The default variant is read first, the aux one after it; Value and
    // Extra are assigned, so they are not inputs, and Spread, read only in
    // aux, comes last. A call's arguments are
    // noted where the call is, before the names inside them that follow.
    CHECK(rows_of(text, "ores_trading_script_inputs_tbl") ==
        "('Swing', ores_utility_system_tenant_id_fn(), 0, 1, 'Notional', "
        "'Number', false, 'Notional > 0', 'the amount'\n"
        "('Swing', ores_utility_system_tenant_id_fn(), 0, 2, 'Dates', "
        "'Event', true, null, null\n"
        "('Swing', ores_utility_system_tenant_id_fn(), 0, 3, 'Pay', "
        "'Event', false, null, null\n"
        "('Swing', ores_utility_system_tenant_id_fn(), 0, 4, 'Ccy', "
        "'Currency', false, null, null\n"
        "('Swing', ores_utility_system_tenant_id_fn(), 0, 5, 'Under', "
        "'Index', false, null, null\n"
        "('Swing', ores_utility_system_tenant_id_fn(), 0, 6, 'Spread', "
        "'Number', false, null, null\n");
    CHECK(text.find("'aux', 'Extra = Spread;', ''") != std::string::npos);
}

TEST_CASE("code holding a newline, a quote or the dollar tag is dollar "
    "quoted with a tag it does not hold", "[codecs][script_library_seed]") {
    const auto out = generate({{library, "<ScriptLibrary><Script><Name>Q"
        "</Name><Script><Code>a = 'b'; $code$ $xcode$</Code></Script>"
        "</Script></ScriptLibrary>"}});
    CHECK(out.at(seed).find("$xxcode$a = 'b'; $code$ $xcode$$xxcode$") !=
        std::string::npos);
}

TEST_CASE("a library that is not XML writes nothing",
    "[codecs][script_library_seed]") {
    CHECK(generate({{library, "<ScriptLibrary>"}}).empty());
}

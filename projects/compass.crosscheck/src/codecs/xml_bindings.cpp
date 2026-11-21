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
#include "compass.crosscheck/codecs/xml_bindings.hpp"

#include <array>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.crosscheck/ops/xml_bindings.hpp"
#include "compass.platform/process/executable.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace xb = ops::xml_bindings;

constexpr std::string_view script =
    "projects/ores.ore/scripts/regenerate_xml_bindings.sh";
constexpr std::string_view xsd = "external/ore/xsd/";
constexpr std::string_view header_dir =
    "projects/ores.ore/core/include/ores.ore.core/xml/generated/";
constexpr std::string_view source_dir =
    "projects/ores.ore/core/src/xml/generated/";
constexpr std::string_view include_dir = "ores.ore.core/xml/generated/";
constexpr std::string_view namespace_prefix = "ores::ore::xml::generated::";

/**
 * @brief One command line of regenerate_xml_bindings.sh. Every family's
 * accessor is its stem and "_document", its namespace and include end in
 * its stem, and its header and source are its stem under the generated
 * directories, so only the stem is stated.
 */
struct family_row {
    std::string_view stem;
    std::string_view root_element;
    std::string_view root_type;
    std::span<const std::string_view> schemas;
    std::span<const std::string_view> exact_decimals = {};
};

constexpr std::array<std::string_view, 2> calendar_adjustments_schemas{
    "calendaradjustment.xsd", "ore_types.xsd"};
constexpr std::array<std::string_view, 2> ibor_fallback_config_schemas{
    "iborfallbackconfig.xsd", "ore_types.xsd"};
constexpr std::array<std::string_view, 3> counterparty_information_schemas{
    "counterparty.xsd", "simulation.xsd", "ore_types.xsd"};
constexpr std::array<std::string_view, 2> todays_market_schemas{
    "todaysmarket.xsd", "ore_types.xsd"};
constexpr std::array<std::string_view, 3> netting_set_definitions_schemas{
    "nettingsetdefinitions.xsd", "instruments.xsd", "ore_types.xsd"};
constexpr std::array<std::string_view, 3> conventions_schemas{
    "conventions.xsd", "intradaypowerloaddata.xsd", "ore_types.xsd"};
constexpr std::array<std::string_view, 2> currency_config_schemas{
    "currencyconfig.xsd", "ore_types.xsd"};
constexpr std::array<std::string_view, 2> pricing_engines_schemas{
    "pricingengines.xsd", "ore_types.xsd"};
constexpr std::array<std::string_view, 2> curve_configuration_schemas{
    "curveconfig.xsd", "ore_types.xsd"};
constexpr std::array<std::string_view, 10> portfolio_schemas{"input.xsd",
    "instruments.xsd", "referencedata.xsd", "simulation.xsd",
    "scriptlibrary.xsd", "simmcalibration.xsd", "historicalreturnconfig.xsd",
    "counterparty.xsd", "intradaypowerloaddata.xsd", "ore_types.xsd"};
constexpr std::array<std::string_view, 1> ore_schemas{"ore.xsd"};
constexpr std::array<std::string_view, 3> stress_testing_schemas{
    "stress.xsd", "sensitivity.xsd", "ore_types.xsd"};
constexpr std::array<std::string_view, 2> credit_simulation_schemas{
    "creditsimulation.xsd", "ore_types.xsd"};

constexpr std::array<std::string_view, 1> spread{"Spread"};
constexpr std::array<std::string_view, 1> every_real{"*"};

/**
 * @brief The families in the order regenerate_xml_bindings.sh generates
 * them. The schemas after the first are read only to resolve what the
 * first reaches; the script's comments say why each is there.
 */
constexpr std::array<family_row, 13> families{{
    {"calendar_adjustments", "CalendarAdjustments", "calendaradjustment",
        calendar_adjustments_schemas},
    {"ibor_fallback_config", "IborFallbackConfig", "IborFallbackConfig",
        ibor_fallback_config_schemas, spread},
    {"counterparty_information", "CounterpartyInformation",
        "counterpartyInformation", counterparty_information_schemas},
    {"todays_market", "TodaysMarket", "todaysmarket", todays_market_schemas},
    {"netting_set_definitions", "NettingSetDefinitions",
        "nettingsetdefinitions", netting_set_definitions_schemas},
    {"conventions", "Conventions", "conventions", conventions_schemas},
    {"currency_config", "CurrencyConfig", "currencyConfig",
        currency_config_schemas},
    {"pricing_engines", "PricingEngines", "pricingengines",
        pricing_engines_schemas},
    {"curve_configuration", "CurveConfiguration", "curveconfiguration",
        curve_configuration_schemas, spread},
    {"portfolio", "Portfolio", "portfolio", portfolio_schemas, every_real},
    {"ore", "ORE", "ore", ore_schemas},
    {"stress_testing", "StressTesting", "stresstesting",
        stress_testing_schemas},
    {"credit_simulation", "CreditSimulation", "creditsimulation",
        credit_simulation_schemas}}};

void run(const ct::corpus_view& v, ct::file_output& out) {
    for (const auto& row : families) {
        const std::string stem(row.stem);
        const auto header = std::string(header_dir) + stem + ".hpp";
        const auto source = std::string(source_dir) + stem + ".cpp";
        std::vector<std::string> texts;
        std::string missing;
        for (const auto name : row.schemas) {
            const auto path = std::string(xsd) + std::string(name);
            auto text = v.read_file(path);
            if (!text) {
                missing = path;
                break;
            }
            texts.push_back(std::move(*text));
        }
        if (!missing.empty()) {
            co::refuse(out, header, "cannot read the schema " + missing +
                    " of the " + stem + " family",
                "Restore the schema, or update the family table of "
                "xml_bindings with " + std::string(script) + ".");
            continue;
        }
        const auto accessor = stem + "_document";
        const auto name_space = std::string(namespace_prefix) + stem;
        const auto include = std::string(include_dir) + stem + ".hpp";
        try {
            auto files = xb::generate(texts, {.root_element = row.root_element,
                .root_type = row.root_type, .accessor = accessor,
                .name_space = name_space, .include = include,
                .exact_decimals = row.exact_decimals});
            out.files.add(header, std::move(files.header));
            out.files.add(source, std::move(files.source));
        } catch (const xb::error& e) {
            co::refuse(out, header, "the schemas of the " + stem +
                    " family do not generate: " + e.what(),
                "Correct the schema the reason names, or the generator if "
                "the shape is one ORE now writes.");
        }
    }
}

constexpr std::array<ct::option, 2> options{{
    {"clang_format", "clang-format",
        "The clang-format the generator runs over the records and tables, a "
        "program on PATH or a path under the checkout; empty for none."},
    {"clang_format_style", ".clang-format",
        "The style file under the checkout clang-format reads."}}};

std::vector<ct::formatter> formatters_of(const ct::option_values& o,
    const std::filesystem::path& checkout) {
    const auto name = o["clang_format"];
    if (name.empty())
        return {};
    const auto program = name.find('/') == std::string_view::npos
        ? compass::platform::process::find_program(name)
        : compass::platform::process::find_program(
            (checkout / name).generic_string());
    if (!program)
        throw std::runtime_error("xml_bindings: " + std::string(name) +
            " is not a program, for the option clang_format");
    return {{.name = "clang-format", .program = *program,
        .extensions = {".cpp", ".hpp"},
        .arguments = {"-i", "--style=file:{style}"}, .passes = 1,
        .style = checkout / std::string(o["clang_format_style"])}};
}

}

const ct::descriptor xml_bindings{
    .name = "xml_bindings",
    .version = 1,
    .summary = "Writes the C++ records and codec tables of each ORE document "
               "family from ORE's schemas.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = co::refusal_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{run},
    .formatters = formatters_of};

}

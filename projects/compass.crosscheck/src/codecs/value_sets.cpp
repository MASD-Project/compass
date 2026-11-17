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
#include "compass.crosscheck/codecs/value_sets.hpp"

#include <array>
#include <set>
#include <string>
#include <string_view>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/value_sets.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;
namespace vs = compass::crosscheck::ops::value_sets;

void write(const ct::corpus_view& v, ct::file_output& out) {
    const auto r = vs::evaluate(v);
    if (r.stopped || !r.problems.empty())
        return;
    out.files.add(vs::seed_path, vs::render_seed(r.data));
    if (!r.rows || !r.columns)
        return;
    out.files.add(vs::columns_path, vs::render_columns(*r.columns));
    out.files.add(vs::map_path, vs::render_map(*r.rows));
}

constexpr std::array<ct::rule, 10> rules{{
    {"unresolved", "The value set curation resolves: every file reads, and "
        "no generator stops on it."},
    {"unlinked_parameter", "Every parameter a screen draws names a value "
        "set, by a curated link."},
    {"clash", "A convention, curve or engine run settings set or link does "
        "not restate one another source holds, and names a set that exists."},
    {"unknown_set", "Every link names a set the curation states."},
    {"unnamed_set", "Every set is named by a link, or generated for a leaf."},
    {"impossible_set", "A set states only a rule its own kind can hold."},
    {"unruled_leaf", "Every leaf of the element map names an authority for "
        "its rule, beyond the count UNREAD_ALLOWED permits."},
    {"override_column", "No pair setting table, model or reader states a "
        "per-pair control the value set states."},
    {"value_check", "No engine settings part states a value as a column "
        "check."},
    {"missing_file", "Every file the override rules guard exists."}}};

std::uint32_t rule_of(std::string_view name) {
    for (std::uint32_t k = 0; k < rules.size(); ++k)
        if (rules[k].name == name)
            return k;
    return 0;
}

/**
 * @brief The files check_parameter_value_sets.py guards against a per-pair
 * override, and the columns each must not state.
 */
struct guarded {
    std::string_view path;
    std::array<std::string_view, 4> columns;
    std::size_t count;
};

constexpr std::array<guarded, 4> guarded_files{{
    {"projects/ores.sql/create/analytics/analytics_pricing_pair_settings_create.sql",
        {"kind", "options"}, 2},
    {"projects/ores.analytics/modeling/ores.analytics.pricing_pair_setting.org",
        {"kind", "options"}, 2},
    {"projects/ores.sql/create/reporting/reporting_engine_settings_group_settings_create.sql",
        {"kind", "options", "minimum", "maximum"}, 4},
    {"projects/ores.reporting/modeling/ores.reporting.engine_settings_group_setting.org",
        {"kind", "options", "minimum", "maximum"}, 4}}};

constexpr std::array<std::string_view, 6> no_value_checks{
    "projects/ores.sql/create/reporting/reporting_engine_settings_fixings_create.sql",
    "projects/ores.sql/create/reporting/reporting_engine_settings_building_create.sql",
    "projects/ores.sql/create/reporting/reporting_engine_settings_execution_create.sql",
    "projects/ores.reporting/modeling/ores.reporting.engine_settings_fixings.org",
    "projects/ores.reporting/modeling/ores.reporting.engine_settings_building.org",
    "projects/ores.reporting/modeling/ores.reporting.engine_settings_execution.org"};

constexpr std::array<std::pair<std::string_view, std::string_view>, 2>
    no_override_reads{{
        {"projects/ores.web/packages/bff/src/pricing-engines.ts",
            R"(row\['(kind|options)'\]|kindSchema)"},
        {"projects/ores.web/packages/web/src/api/pricingEngines.ts",
            R"(z\.enum\(\s*\[\s*'flag')"}}};

void override_rules(const ct::corpus_view& v, ct::diagnostic_table& out) {
    const auto missing = [&](std::string_view path) {
        co::report(out, path, 0, rule_of("missing_file"), std::string(path) +
            ": does not exist", "Restore the file, or drop it from the rule.");
    };
    for (const auto& g : guarded_files) {
        const auto bytes = v.read_file(g.path);
        if (!bytes) {
            missing(g.path);
            continue;
        }
        for (std::size_t k = 0; k < g.count; ++k) {
            const auto column = std::string(g.columns[k]);
            auto at = bytes->find("\"" + column + "\"");
            if (at == std::string::npos)
                at = bytes->find("** " + column);
            if (at == std::string::npos)
                continue;
            co::report(out, g.path, static_cast<std::uint32_t>(at),
                rule_of("override_column"), std::string(g.path) +
                ": states the override column " + column, "State the rule in "
                "the setting's value set; a per-pair column has no place here.");
        }
    }
    static const pt::regex value_check(
        R"(check\s*\([^)]*(>=|<=|\bin\s*\(|\bbetween\b))", {.ignorecase = true});
    for (const auto path : no_value_checks) {
        const auto bytes = v.read_file(path);
        if (!bytes) {
            missing(path);
            continue;
        }
        const auto text = pt::universal_newlines(pt::decode_replacing(*bytes));
        std::size_t start = 0;
        for (std::uint32_t line = 1; start <= text.size(); ++line) {
            const auto end = std::min(text.find('\n', start), text.size());
            if (value_check.search(std::string_view(text).substr(start,
                    end - start)))
                co::report(out, path, co::offset_of_line(*bytes, line),
                    rule_of("value_check"), std::string(path) + ": states a "
                    "value as a column check; the set states it", "Drop the "
                    "check and state the bound in the setting's value set.");
            start = end + 1;
        }
    }
    for (const auto& [path, pattern] : no_override_reads) {
        const auto bytes = v.read_file(path);
        if (!bytes) {
            missing(path);
            continue;
        }
        const pt::regex re(pattern);
        std::set<std::string> seen;
        for (const auto& m : re.find_all(*bytes)) {
            const auto column = std::string(m.groups() > 0 && m.matched(1)
                ? m.group(1) : m.group());
            if (!seen.insert(column).second)
                continue;
            co::report(out, path, static_cast<std::uint32_t>(m.start()),
                rule_of("override_column"), std::string(path) +
                ": reads the retired pair setting " + column, "Read the rule "
                "from the setting's value set.");
        }
    }
}

void check(const ct::corpus_view& v, ct::diagnostic_table& out) {
    const auto r = vs::evaluate(v);
    if (r.stopped) {
        co::report(out, vs::generator_path, 0, rule_of("unresolved"),
            "the value sets cannot be resolved: " + *r.stopped, "Correct the "
            "curation the message names.");
    }
    for (const auto& p : r.problems)
        co::report(out, vs::generator_path, 0, rule_of(p.rule), p.message,
            "Correct the curation, then regenerate the seed.");
    if (!r.stopped && r.problems.empty() && r.map_stopped)
        co::report(out, vs::map_generator_path, 0, rule_of("unresolved"),
            "the element map cannot be built: " + *r.map_stopped, "Correct "
            "the source the message names.");
    if (r.rows) {
        std::vector<const vs::map_row*> gap;
        for (const auto& row : *r.rows)
            if (row.source == "ore_schema")
                gap.push_back(&row);
        if (static_cast<long long>(gap.size()) > r.unread_allowed)
            for (const auto* row : gap)
                co::report(out, vs::map_generator_path, 0, rule_of("unruled_leaf"),
                    row->schema + "|" + row->xpath + " names no authority for "
                    "its rule", "Teach the extractor the form the leaf is read "
                    "in, or state the rule another way.");
    }
    override_rules(v, out);
}

}

const ct::descriptor ore_value_sets{
    .name = "ore_value_sets",
    .version = 1,
    .summary = "Writes the parameter value set seed, the ORE element map and "
               "its columns from one resolution of the value set curation.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = {},
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{write}};

const ct::descriptor parameter_value_sets{
    .name = "parameter_value_sets",
    .version = 1,
    .summary = "Reports every rule the parameter value sets break, from the "
               "same resolution the seed is written from.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{check}};

}

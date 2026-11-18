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
#include "compass.crosscheck/codecs/registry_gates.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/python_literals.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

std::string read(const ct::corpus_view& v, std::string_view path) {
    return pt::universal_newlines(pt::decode_replacing(
        v.read_file(path).value_or("")));
}

constexpr std::array<std::string_view, 2> bad_exception_sources{
    "E22B9A3C-DF20-4EC3-8BEC-C193E1560745",
    "7B477025-132E-4B20-80B4-D8C9420880B8"};

constexpr std::array<ct::rule, 2> exception_rules{{
    {"bad_exception", "Each accepted exception of the component registry "
        "names a listed component, a checklist item the standard defines "
        "once, a reason, who accepted it and an ISO date.",
        bad_exception_sources},
    {"unreadable_registry", "The registry and the standard state their "
        "tables in a form this check reads."}}};

constexpr std::array<ct::option, 2> exception_options{{
    {"registry", "projects/ores.codegen/scripts/component_registry.py",
        "The Python module whose COMPONENTS_UNDER_TEST and "
        "ACCEPTED_EXCEPTIONS the check reads."},
    {"standard", "doc/knowledge/architecture/component_clean_standard.org",
        "The page whose checklist tables define the items."}}};

std::string field(const ops::python_value& call, std::string_view key) {
    for (const auto& [k, v, tuple] : call.entries) {
        if (k == key && v.what == ops::python_value::kind::string)
            return v.text;
    }
    return {};
}

void registry_exception_run(const ct::corpus_view& v,
    ct::diagnostic_table& out) {
    static const pt::regex item_row(R"(^\|\s*([A-Z]\d{2})\s*\|)",
        {.multiline = true});
    static const pt::regex date(R"(^\d{4}-\d{2}-\d{2}$)");
    const auto registry_path = v.option("registry");
    const auto standard_path = v.option("standard");
    std::set<std::string> items;
    const auto standard = read(v, standard_path);
    for (const auto& m : item_row.find_all(standard))
        items.emplace(m.group(1));
    if (items.empty()) {
        co::report(out, standard_path, 0, 1, "no checklist items found in "
            "the standard", "Keep the standard's checklist rows as "
            "| B01 | ... |.");
        return;
    }
    const auto module = read(v, registry_path);
    const auto listed_value = ops::python_assignment(module,
        "COMPONENTS_UNDER_TEST");
    const auto listed = listed_value ? ops::strings_of(*listed_value)
                                     : std::nullopt;
    const auto accepted = ops::python_assignment(module,
        "ACCEPTED_EXCEPTIONS");
    if (!listed || !accepted ||
        accepted->what != ops::python_value::kind::dict) {
        co::report(out, registry_path, 0, 1, "COMPONENTS_UNDER_TEST or "
            "ACCEPTED_EXCEPTIONS is not a literal this check can read",
            "Keep both as literals of strings and AcceptedException calls.");
        return;
    }
    const auto problem = [&](const std::string& message) {
        co::report(out, registry_path, 0, 0, message, "Correct the entry in "
            "ACCEPTED_EXCEPTIONS.");
    };
    auto entries = accepted->entries;
    std::ranges::sort(entries, {}, [](const auto& e) { return e.key; });
    for (const auto& [component, exceptions, tuple] : entries) {
        if (std::ranges::find(*listed, component) == listed->end())
            problem(component + ": carries accepted exceptions but is not "
                "listed in COMPONENTS_UNDER_TEST; an exception only means "
                "something for a component the gates check");
        std::set<std::string> seen;
        for (const auto& entry : exceptions.items) {
            const auto item = field(entry, "item");
            const auto where = component + " " + item;
            if (!items.contains(item))
                problem(where + ": not an item the standard defines");
            if (seen.contains(item))
                problem(where + ": recorded twice");
            seen.insert(item);
            if (co::strip(field(entry, "reason")).empty())
                problem(where + ": states no reason");
            if (co::strip(field(entry, "accepted_by")).empty())
                problem(where + ": names nobody as having accepted it");
            const auto on = field(entry, "accepted_on");
            if (!date.match(co::strip(on)))
                problem(where + ": accepted_on is '" + on + "', not an ISO "
                    "date");
        }
    }
}

}

const ct::descriptor registry_exceptions{
    .name = "registry_exceptions",
    .version = 1,
    .summary = "Reports each accepted exception of the component registry "
        "that is not justified.",
    .reads = ct::inputs::of_types({}),
    .options = exception_options,
    .rules = exception_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{registry_exception_run}};

}

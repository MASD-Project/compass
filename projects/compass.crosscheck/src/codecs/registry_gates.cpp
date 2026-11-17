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
#include <map>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/paths.hpp"
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

constexpr std::array<ct::rule, 2> area_rules{{
    {"unpublished_area", "Every #+http_area: a model declares is a "
        "published area."},
    {"unreadable_areas", "The route classification module states AREAS "
        "and NO_AREA as literals."}}};

constexpr std::array<ct::option, 1> area_options{{
    {"classification", "build/scripts/route_classification.py",
        "The Python module whose AREAS and NO_AREA name the published "
        "areas."}}};

void http_area_run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex declaration(R"(^#\+http_area:\s*(\S+)\s*$)",
        {.multiline = true});
    const auto module_path = v.option("classification");
    const auto module = read(v, module_path);
    const auto areas = ops::python_assignment(module, "AREAS");
    const auto none = ops::python_assignment(module, "NO_AREA");
    const auto names = areas ? ops::strings_of(*areas) : std::nullopt;
    if (!names || !none || none->what != ops::python_value::kind::string) {
        co::report(out, module_path, 0, 1, "AREAS or NO_AREA is not a "
            "literal this check can read", "Keep AREAS a tuple of strings "
            "and NO_AREA a string.");
        return;
    }
    std::set<std::string> published(names->begin(), names->end());
    published.erase(none->text);
    std::string listed;
    for (const auto& a : published)
        listed += (listed.empty() ? "" : ", ") + a;
    for (const auto path : co::glob(v, "projects/*/modeling/*.org")) {
        const auto bytes = v.read_file(path).value_or("");
        const auto text = pt::universal_newlines(pt::decode_replacing(bytes));
        for (const auto& m : declaration.find_all(text)) {
            const std::string area(m.group(1));
            if (published.contains(area))
                continue;
            co::report(out, path, co::offset_of_line(bytes,
                co::line_of(text, m.start())), 0, "declares http_area '" +
                area + "', which is not a published area; published areas: " +
                listed, "Add the area to route_classification.AREAS, or use "
                "a published one.");
        }
    }
}

constexpr std::array<ct::rule, 3> route_rules{{
    {"unregistered_unit", "A component's route aggregate registers the "
        "route unit of every model that opts into one."},
    {"twice_registered", "An aggregate registers each unit once."},
    {"missing_unit", "Every unit an aggregate registers has its source in "
        "the component."}}};

void route_unit_run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex enabled(
        R"(^:ores\.cpp\.http-route\.enabled:\s*true\s*$)",
        {.multiline = true});
    static const pt::regex singular(
        R"(^#\+(entity_singular|name_singular):\s*(\S+)\s*$)",
        {.multiline = true});
    static const pt::regex registers(R"(^\s*(\w+)_routes::register_routes\()");
    constexpr std::string_view adapter = "projects/ores.http/";
    std::set<std::string> components;
    for (const auto p : v.list_files(adapter)) {
        const auto parts = co::components_of(p);
        if (parts.size() > 3)
            components.emplace(parts[2]);
    }
    for (const auto& c : components) {
        const auto dir = std::string(adapter) + c;
        const auto aggregate = dir + "/src/routes/" + c + "/" + c +
            "_routes.cpp";
        if (!v.file_exists(aggregate))
            continue;
        std::map<std::string, std::string_view> expected;
        for (const auto model : co::glob(v, "projects/ores." + c +
                 "/modeling/*.org")) {
            const auto text = read(v, model);
            if (!enabled.search(text))
                continue;
            if (const auto s = singular.search(text))
                expected[std::string(s->group(2))] = model;
        }
        std::vector<std::string> called;
        for (const auto line : co::split_lines(read(v, aggregate))) {
            if (const auto m = registers.match(line))
                called.emplace_back(m->group(1));
        }
        for (const auto& [unit, model] : expected) {
            if (std::ranges::find(called, unit) == called.end())
                co::report(out, aggregate, 0, 0, "never registers " + unit +
                    "_routes, which " + std::string(model) + " opts into; "
                    "the gateway answers nothing for it and nothing reports "
                    "it", "Register the unit, or remove the model's "
                    ":ores.cpp.http-route.enabled: opt-in.");
        }
        std::vector<std::string> seen;
        for (const auto& unit : called) {
            if (std::ranges::find(seen, unit) != seen.end())
                continue;
            seen.push_back(unit);
            const auto n = std::ranges::count(called, unit);
            if (n > 1)
                co::report(out, aggregate, 0, 1, "registers " + unit +
                    "_routes " + std::to_string(n) + " times; the second "
                    "registration claims routes the first stated",
                    "Register each unit once.");
        }
        for (const auto& unit : called) {
            if (!v.file_exists(dir + "/src/routes/" + c + "/" + unit +
                    "_routes.cpp"))
                co::report(out, aggregate, 0, 2, "registers " + unit +
                    "_routes, which has no unit in this component",
                    "Remove the registration, or restore the unit.");
        }
    }
}

constexpr std::array<ct::rule, 2> exception_rules{{
    {"bad_exception", "Each accepted exception of the component registry "
        "names a listed component, a checklist item the standard defines "
        "once, a reason, who accepted it and an ISO date."},
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

const ct::descriptor http_areas{
    .name = "http_areas",
    .version = 1,
    .summary = "Reports each HTTP area a model declares that is not "
        "published.",
    .reads = ct::inputs::of_types({}),
    .options = area_options,
    .rules = area_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{http_area_run}};

const ct::descriptor route_units{
    .name = "route_units",
    .version = 1,
    .summary = "Reports each route unit a component opts into that its "
        "aggregate does not register once.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = route_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{route_unit_run}};

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

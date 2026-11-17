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
#include "compass.crosscheck/codecs/protocol_twins.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codegen/ops/manifest.hpp"
#include "compass.codegen/ops/physical_space.hpp"
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/python_literals.hpp"
#include "compass.platform/text/decoding.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace cg = compass::codegen::ops;
namespace pt = compass::platform::text;

enum rule : std::uint32_t { missing_twin, opt_out, unreadable_inputs };

constexpr std::array<ct::rule, 3> rules{{
    {"missing_twin", "Every generated protocol header of a listed component "
        "with committed TypeScript protocol output has its TypeScript "
        "twin."},
    {"opt_out", "No model switches the TypeScript protocol facet off."},
    {"unreadable_inputs", "The template library and the component registry "
        "read as the check expects."}}};

constexpr std::array<ct::option, 2> options{{
    {"library", "projects/ores.codegen/library",
        "The template library: its templates/ores.*.org graph and its "
        "component_catalogue.org."},
    {"registry", "projects/ores.codegen/scripts/component_registry.py",
        "The Python module whose COMPONENTS_UNDER_TEST lists the checked "
        "components."}}};

std::string read(const ct::corpus_view& v, std::string_view path) {
    return pt::universal_newlines(pt::decode_replacing(
        v.read_file(path).value_or("")));
}

/**
 * @brief The output patterns of the ores.ts archetypes that render a
 * protocol twin, in the graph's order of facets.
 */
std::vector<std::string> twin_patterns(const ct::corpus_view& v,
    const std::string& library) {
    const auto templates = library + "/templates";
    std::vector<std::string> names;
    std::vector<std::string> texts;
    for (const auto path : v.list_files(templates + "/ores.", ".org")) {
        if (path.find('/', templates.size() + 1) != std::string_view::npos)
            continue;
        names.emplace_back(co::components_of(path).back());
        texts.push_back(read(v, path));
    }
    std::vector<cg::node_document> nodes;
    for (std::size_t i = 0; i < names.size(); ++i)
        nodes.push_back({names[i], texts[i]});
    const auto graph = cg::load_physical_space(nodes);
    const auto& a = graph.archetypes;
    std::vector<std::size_t> order(a.size());
    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::ranges::stable_sort(order, [&](auto x, auto y) {
        return a.facets[x] < a.facets[y];
    });
    std::vector<std::string> out;
    for (const auto i : order) {
        if (a.facets[i].starts_with("ores.ts.") &&
            a.outputs[i].ends_with("_protocol.ts") &&
            a.outputs[i].contains("{component}"))
            out.push_back(a.outputs[i]);
    }
    return out;
}

std::string replaced(std::string text, std::string_view from,
    std::string_view to) {
    for (auto at = text.find(from); at != std::string::npos;
         at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
    return text;
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    const std::string library(v.option("library"));
    const auto registry_path = v.option("registry");
    const auto listed_value = ops::python_assignment(read(v, registry_path),
        "COMPONENTS_UNDER_TEST");
    const auto listed = listed_value ? ops::strings_of(*listed_value)
                                     : std::nullopt;
    if (!listed) {
        co::report(out, registry_path, 0, unreadable_inputs,
            "COMPONENTS_UNDER_TEST is not a literal this check can read",
            "Keep COMPONENTS_UNDER_TEST a tuple of strings.");
        return;
    }
    const auto patterns = twin_patterns(v, library);
    const auto catalogue = cg::read_component_catalogue(read(v,
        library + "/component_catalogue.org"));

    std::set<std::pair<std::string, std::string>> seen;
    for (const auto& component : catalogue) {
        std::vector<std::string> dir_names{component.name};
        if (component.name.ends_with("-cpp"))
            dir_names.push_back(component.name.substr(0,
                component.name.size() - 4));
        if (std::ranges::none_of(dir_names, [&](const auto& n) {
                return std::ranges::find(*listed, n) != listed->end(); }))
            continue;
        if (component.modeling_dir.empty())
            continue;
        const std::string project(co::parent_of(component.modeling_dir));
        std::vector<std::string> committed;
        for (const auto& pattern : patterns) {
            for (const auto& name : dir_names) {
                const auto output = replaced(pattern, "{component}", name);
                const std::string dir(co::parent_of(output));
                if (v.list_files(dir + "/").empty() ||
                    std::ranges::find(committed, dir) != committed.end())
                    continue;
                committed.push_back(dir);
            }
        }
        for (const auto& ts : committed) {
            if (!seen.emplace(ts, project).second)
                continue;
            for (const auto header : co::glob(v, project +
                     "/api/include/*/messaging/*_protocol.hpp")) {
                const auto twin = ts + "/" + std::string(co::stem_of(header)) +
                    ".ts";
                if (v.file_exists(twin))
                    continue;
                co::report(out, header, 0, missing_twin, "expected " + twin +
                    " (component '" + component.name + "')", "Regenerate "
                    "the component's TypeScript protocol facet.");
            }
        }
    }

    constexpr std::string_view key = ":ores.ts.protocol.enabled:";
    for (const auto path : v.list_files("projects/", ".org")) {
        const auto parts = co::components_of(path);
        if (parts.size() < 4 || parts[2] != "modeling")
            continue;
        const auto bytes = v.read_file(path).value_or("");
        if (!bytes.contains(key))
            continue;
        const auto text = pt::universal_newlines(pt::decode_replacing(bytes));
        std::uint32_t number = 0;
        for (const auto line : co::split_lines(text)) {
            ++number;
            if (line.contains(key))
                co::report(out, path, co::offset_of_line(bytes, number),
                    opt_out, "the model opts out of the TypeScript protocol "
                    "facet: " + std::string(co::strip(line)), "Remove the "
                    "key, so no model silences the facet.");
        }
    }
}

}

const ct::descriptor protocol_twins{
    .name = "protocol_twins",
    .version = 1,
    .summary = "Reports each generated protocol header with no TypeScript "
        "twin, and each model that opts out of the twin.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}

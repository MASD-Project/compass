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
#include "compass.bench/ops/render_stage.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <stdexcept>
#include "compass.bench/ops/model_corpus.hpp"
#include "compass.codegen/ops/compile.hpp"
#include "compass.codegen/ops/context.hpp"
#include "compass.codegen/ops/entity_context.hpp"
#include "compass.codegen/ops/entity_registry.hpp"
#include "compass.codegen/ops/manifest.hpp"
#include "compass.codegen/ops/output.hpp"
#include "compass.codegen/ops/physical_space.hpp"
#include "compass.codegen/ops/values.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/builder.hpp"
#include "compass.model/ops/load.hpp"
#include "compass.model/ops/outline.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"

namespace compass::bench::ops {

namespace {

using compass::model::types::model_tables;
using compass::model::types::text_id;
using compass::model::types::value_id;
using compass::model::types::value_kind;
using clock_type = std::chrono::steady_clock;

double elapsed_ms(clock_type::time_point start) {
    return std::chrono::duration<double, std::milli>(
        clock_type::now() - start).count();
}

value_id member(const model_tables& t, value_id object, std::string_view key) {
    for (auto c = t.values.firsts[object]; !c.is_none();
         c = t.values.nexts[c]) {
        if (compass::model::types::text_of(t.texts, t.values.keys[c]) == key)
            return c;
    }
    return value_id::none();
}

std::string_view string_member(const model_tables& t, value_id object,
    std::string_view key) {
    const auto v = member(t, object, key);
    if (v.is_none() || t.values.kinds[v] != value_kind::string)
        throw std::runtime_error("no string " + std::string(key));
    return compass::model::types::text_of(t.texts,
        text_id{static_cast<std::uint32_t>(t.values.scalars[v])});
}

/**
 * @brief Every .mustache template of ores.codegen's library, compiled once
 * and found by its file name.
 */
using template_map =
    std::map<std::string, compass::codegen::types::compiled_template,
        std::less<>>;

template_map compile_templates(const std::filesystem::path& checkout) {
    template_map templates;
    const auto dir = checkout / "projects/ores.codegen/library/templates";
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        const auto name = entry.path().filename().string();
        if (!name.ends_with(".mustache"))
            continue;
        const compass::platform::filesystem::mapped_file file(entry.path());
        templates.emplace(name,
            compass::codegen::ops::compile(std::string(file.text())));
    }
    return templates;
}

void write_file(const std::filesystem::path& target, std::string_view text) {
    std::filesystem::create_directories(target.parent_path());
    std::ofstream os(target, std::ios::binary);
    os << text;
    if (!os)
        throw std::runtime_error("cannot write " + target.string());
}

void write_output(const std::filesystem::path& out, std::string_view path,
    std::string_view text) {
    write_file(out / "renders" / std::filesystem::path(path), text);
}

struct unit {
    std::string output;
    std::string template_name;
};

/**
 * @brief The dump's outputs, grouped by the context they render from, so
 * one context at a time is read and rendered.
 */
std::map<std::string, std::vector<unit>> oracle_units(
    const std::filesystem::path& oracle) {
    const compass::platform::filesystem::mapped_file file(
        oracle / "index.json");
    model_tables t;
    compass::model::ops::builder b(t);
    const auto index = compass::model::io::read_json(b, file.text());
    std::map<std::string, std::vector<unit>> by_context;
    const auto outputs = member(t, index, "outputs_by_path");
    for (auto e = t.values.firsts[outputs]; !e.is_none();
         e = t.values.nexts[e]) {
        by_context[std::string(string_member(t, e, "context"))].push_back(
            {std::string(string_member(t, e, "output")),
             std::string(string_member(t, e, "template"))});
    }
    return by_context;
}

void render_oracle(render_result& r, const template_map& templates,
    const std::filesystem::path& oracle, unsigned repetitions,
    const std::filesystem::path& out) {
    const auto start = clock_type::now();
    const auto by_context = oracle_units(oracle);
    r.setup_ms += elapsed_ms(start);
    r.render_ms.assign(repetitions, 0.0);
    std::string text;
    for (const auto& [digest, units] : by_context) {
        const auto read_start = clock_type::now();
        const compass::platform::filesystem::mapped_file file(
            oracle / "contexts" / (digest + ".json"));
        model_tables t;
        compass::model::ops::builder b(t);
        const auto context = compass::model::io::read_json(b, file.text());
        r.setup_ms += elapsed_ms(read_start);
        ++r.contexts;
        for (const auto& u : units) {
            const auto it = templates.find(u.template_name);
            if (it == templates.end()) {
                r.messages.push_back(u.output + ": no template " +
                    u.template_name);
                continue;
            }
            for (unsigned rep = 0; rep < repetitions; ++rep) {
                text.clear();
                const auto render_start = clock_type::now();
                compass::codegen::ops::render_output(text, u.template_name,
                    it->second, t, context);
                r.render_ms[rep] += elapsed_ms(render_start);
            }
            ++r.outputs;
            r.bytes += text.size();
            if (!out.empty())
                write_output(out, u.output, text);
        }
    }
}

int current_year() {
    const std::chrono::year_month_day today{
        std::chrono::floor<std::chrono::days>(
            std::chrono::system_clock::now())};
    return static_cast<int>(today.year());
}

/**
 * @brief The physical-space graph of the library's ores.*.org nodes, read
 * in file-name order as load_graph reads them.
 */
compass::codegen::types::physical_space load_graph(
    const std::filesystem::path& checkout) {
    const auto dir = checkout / "projects/ores.codegen/library/templates";
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        const auto name = entry.path().filename().string();
        if (name.starts_with("ores.") && name.ends_with(".org"))
            paths.push_back(entry.path());
    }
    std::ranges::sort(paths);
    std::vector<compass::platform::filesystem::mapped_file> files;
    std::vector<std::string> names;
    files.reserve(paths.size());
    for (const auto& p : paths) {
        files.emplace_back(p);
        names.push_back(p.filename().string());
    }
    std::vector<compass::codegen::ops::node_document> nodes;
    for (std::size_t i = 0; i < files.size(); ++i)
        nodes.push_back({names[i], files[i].text()});
    return compass::codegen::ops::load_physical_space(nodes);
}

/**
 * @brief The registry of the checkout's models resolve_targets consults,
 * read from every .org file under a modeling/ directory of projects/.
 */
compass::codegen::types::checkout_registry load_registry(
    const std::filesystem::path& checkout, const types::file_table& table) {
    std::vector<compass::platform::filesystem::mapped_file> files;
    std::vector<std::string_view> paths;
    for (const auto& path : table.paths) {
        if (!path.starts_with("projects/") || !path.ends_with(".org") ||
            path.find("/modeling/") == std::string::npos)
            continue;
        files.emplace_back(checkout / std::filesystem::path(path));
        paths.push_back(path);
    }
    std::vector<compass::codegen::ops::checkout_file> documents;
    for (std::size_t i = 0; i < files.size(); ++i)
        documents.push_back({paths[i], files[i].text()});
    return compass::codegen::ops::read_checkout_registry(documents);
}

/**
 * @brief The registry of the entity models a domain entity's enrichment
 * reads beside its own: those of projects/<component>/modeling/, scanned
 * in path order as _entity_org_by_table scans them, a later claim of a
 * table winning, each loaded into tables of its own; the routes of
 * the trade type catalogues there; and the custom-type registry.
 */
compass::codegen::types::entity_registry load_entity_registry(
    const types::file_table& table, model_corpus& documents,
    const compass::model::ops::document_finder& find,
    compass::model::ops::load_cache& cache) {
    namespace mops = compass::model::ops;
    using parts = std::pair<std::string_view, std::string_view>;
    std::vector<std::pair<parts, std::string_view>> paths;
    constexpr std::string_view projects = "projects/";
    constexpr std::string_view modeling = "/modeling/";
    for (const auto& path : table.paths) {
        if (!path.starts_with(projects) || !path.ends_with(".org"))
            continue;
        const std::string_view rest(path.data() + projects.size(),
            path.size() - projects.size());
        const auto at = rest.find(modeling);
        if (at == std::string_view::npos || at == 0 ||
            rest.find('/', at + modeling.size()) != std::string_view::npos ||
            rest.substr(0, at).find('/') != std::string_view::npos)
            continue;
        paths.push_back({{rest.substr(0, at), rest.substr(at + modeling.size())},
            path});
    }
    std::ranges::sort(paths);

    compass::codegen::types::entity_registry registry;
    registry.custom_types = mops::custom_type_headers(find, cache);
    model_tables scratch;
    mops::builder b(scratch);
    mops::outline outline;
    for (const auto& [key, path] : paths) {
        auto* p = documents.get(path);
        if (p == nullptr)
            continue;
        const auto doc = document_of(*p);
        const auto type = mops::model_type_of(mops::declared_type(doc));
        if (type == compass::model::types::model_type::trade_type_catalogue) {
            mops::build_outline(p->file.text(), p->view, outline);
            registry.trade_type_routes[
                std::filesystem::path(key.second).stem().string()] =
                compass::codegen::ops::trade_type_routes_of(outline);
        }
        const auto claim =
            compass::codegen::ops::entity_table_claim_of(p->file.text());
        if (!claim)
            continue;
        auto& owner = registry.tables[claim->table];
        owner = {claim->entity_singular, std::nullopt};
        if (!type)
            continue;
        std::string error;
        const auto id = mops::load_model(b, doc, *type, find, cache, &error);
        if (error.empty())
            owner.parent = compass::codegen::ops::parent_entity_of(scratch,
                scratch.models.roots[id]);
    }

    for (const auto& path : table.paths) {
        if (!path.starts_with(projects) || !path.ends_with(".org") ||
            path.find(modeling) == std::string::npos)
            continue;
        auto* p = documents.get(path);
        if (p == nullptr)
            continue;
        compass::codegen::ops::add_directory_model(
            registry.directories[path.substr(0, path.rfind('/'))],
            p->file.text());
    }

    for (const auto& path : table.paths) {
        if (path.starts_with(projects) && path.ends_with(".hpp") &&
            path.find("/domain/") != std::string::npos)
            registry.domain_headers.insert(path);
    }

    constexpr std::string_view group_suffix = "_field_group.org";
    for (const auto& path : table.paths) {
        if (!path.starts_with(projects) || !path.ends_with(group_suffix) ||
            path.find(modeling) == std::string::npos)
            continue;
        auto* p = documents.get(path);
        if (p == nullptr)
            continue;
        std::string error;
        const auto id = mops::load_model(b, document_of(*p),
            compass::model::types::model_type::field_group, find, cache,
            &error);
        if (!error.empty())
            continue;
        const auto& t = b.tables();
        const auto group = compass::codegen::ops::values::member(t,
            scratch.models.roots[id], "field_group");
        if (!compass::codegen::ops::values::truthy(t, group, "has_economic_fields"))
            continue;
        const auto name = std::filesystem::path(path).filename().string();
        registry.economic_field_groups.insert(
            name.substr(0, name.size() - group_suffix.size()));
    }
    return registry;
}

struct planned {
    compass::codegen::ops::unit unit;
    value_id context;
    std::string_view type;
};

/**
 * @brief The model types compass.codegen builds whole contexts for, with
 * their ores.codegen names.
 */
struct context_type {
    compass::model::types::model_type type;
    std::string_view name;
};

constexpr context_type context_types[] = {
    {compass::model::types::model_type::entity, "domain_entity"},
    {compass::model::types::model_type::component, "component"},
    {compass::model::types::model_type::asset_class_catalogue,
     "asset_class_catalogue"},
    {compass::model::types::model_type::outcome_catalogue,
     "outcome_catalogue"}};

/**
 * @brief The outputs compass builds contexts for, each with its context:
 * those of the models regenerate --all visits whose type is one of
 * context_types, as resolve_targets lists them.
 */
std::vector<planned> plan_models(const std::filesystem::path& checkout,
    const types::file_table& table, compass::model::ops::builder& b,
    render_result& r) {
    namespace mops = compass::model::ops;
    const auto data = checkout / "projects/ores.codegen/library/data";
    const compass::platform::filesystem::mapped_file licence(
        data / "licence-GPL-v3.txt");
    const compass::platform::filesystem::mapped_file modelines(
        data / "modeline.org");
    const auto library = compass::codegen::ops::library_values(b,
        licence.text(), modelines.text(), current_year());
    const auto graph = load_graph(checkout);
    const auto registry = load_registry(checkout, table);

    model_corpus documents(checkout);
    const compass::platform::filesystem::mapped_file catalogue(
        checkout / "projects/ores.codegen/library/component_catalogue.org");
    const auto regenerated = compass::codegen::ops::regenerated_models(
        compass::codegen::ops::read_component_catalogue(catalogue.text()),
        table.paths, [&](std::string_view path) {
            auto* p = is_model_candidate(path) ? documents.get(path) : nullptr;
            return p == nullptr ? std::string()
                : std::string(mops::declared_type(document_of(*p)));
        });

    const auto find = documents.finder();
    mops::load_cache cache;
    const auto entities = load_entity_registry(table, documents, find, cache);
    mops::outline outline;
    std::vector<planned> plan;
    for (const auto& path : table.paths) {
        if (!regenerated.contains(path))
            continue;
        auto* p = documents.get(path);
        if (p == nullptr)
            continue;
        const auto type = mops::model_type_of(
            mops::declared_type(document_of(*p)));
        const auto known = std::ranges::find_if(context_types,
            [&](const context_type& c) { return type && c.type == *type; });
        if (known == std::end(context_types))
            continue;
        const auto type_name = known->name;
        std::string error;
        const auto id = mops::load_model(b, document_of(*p), *type, find,
            cache, &error);
        if (!error.empty()) {
            r.messages.push_back(path + ": " + error);
            continue;
        }
        const auto root = b.tables().models.roots[id];
        mops::build_outline(p->file.text(), p->view, outline);
        std::vector<compass::codegen::ops::drawer_property> properties;
        for (std::size_t i = 0; i < outline.file_property_keys.size(); ++i)
            properties.push_back({outline.file_property_keys[i],
                outline.file_property_values[i]});
        const auto overrides = mops::physical_space_overrides(outline, find,
            cache);
        for (const auto& [key, enabled] : overrides)
            properties.push_back({key, enabled ? "true" : "false"});
        const auto units = compass::codegen::ops::model_units(graph, registry,
            type_name, b.tables(), root, path, properties);
        if (units.empty())
            continue;
        const auto stem = std::filesystem::path(path).stem().string();
        constexpr std::string_view wire_codec_addresses[] = {
            "ores.cpp.wire-codec.codec_header",
            "ores.cpp.wire-codec.codec_impl",
            "ores.cpp.wire-codec.protocol_codec_impl"};
        constexpr std::string_view protocol_addresses[] = {"ores.cpp.protocol"};
        const compass::codegen::types::entity_site site{path,
            compass::codegen::ops::facet_enabled(properties,
                "ores.cpp.wire-codec", wire_codec_addresses, false),
            compass::codegen::ops::facet_enabled(properties,
                "ores.cpp.protocol", protocol_addresses, true)};
        const auto context = *type == compass::model::types::model_type::entity
            ? compass::codegen::ops::entity_context(b, library, root, stem,
                entities, site)
            : compass::codegen::ops::model_context(b, library, root, stem,
                type_name);
        ++r.contexts;
        for (const auto& u : units)
            plan.push_back({u, context, type_name});
    }
    return plan;
}

/**
 * @brief Writes each distinct context of @p plan once, as
 * <out>/contexts/<n>.json, and <out>/context_index.json, which names for
 * every output its context and its model type, for
 * build/scripts/codegen_render_diff.py.
 */
void write_contexts(const std::filesystem::path& out, const model_tables& t,
    const std::vector<planned>& plan) {
    std::map<std::uint32_t, std::size_t> numbers;
    std::string index = "[\n";
    std::string text;
    for (std::size_t i = 0; i < plan.size(); ++i) {
        const auto& p = plan[i];
        const auto [it, added] = numbers.try_emplace(p.context.value,
            numbers.size());
        const auto number = std::to_string(it->second);
        if (added) {
            text.clear();
            compass::model::io::write_json(text, t, p.context);
            write_file(out / "contexts" / (number + ".json"), text);
        }
        index += "{\"output\": \"" + p.unit.output + "\", \"context\": \"" +
            number + "\", \"type\": \"" + std::string(p.type) + "\"}" +
            (i + 1 < plan.size() ? ",\n" : "\n");
    }
    write_file(out / "context_index.json", index + "]\n");
}

void render_models(render_result& r, const template_map& templates,
    const std::filesystem::path& checkout, const types::file_table& table,
    unsigned repetitions, const std::filesystem::path& out) {
    const auto start = clock_type::now();
    model_tables t;
    compass::model::ops::builder b(t);
    const auto plan = plan_models(checkout, table, b, r);
    r.setup_ms = elapsed_ms(start);
    if (!out.empty())
        write_contexts(out, t, plan);

    r.render_ms.assign(repetitions, 0.0);
    std::string text;
    for (const auto& [u, context, type] : plan) {
        const auto it = templates.find(u.template_name);
        if (it == templates.end()) {
            r.messages.push_back(u.output + ": no template " +
                u.template_name);
            continue;
        }
        for (unsigned rep = 0; rep < repetitions; ++rep) {
            text.clear();
            const auto render_start = clock_type::now();
            compass::codegen::ops::render_output(text, u.template_name,
                it->second, t, context);
            r.render_ms[rep] += elapsed_ms(render_start);
        }
        ++r.outputs;
        r.bytes += text.size();
        if (!out.empty())
            write_output(out, u.output, text);
    }
}

}

render_result run_render_stage(const std::filesystem::path& checkout,
    const types::file_table& table, const std::filesystem::path& oracle,
    unsigned repetitions, const std::filesystem::path& out) {
    render_result r;
    const auto start = clock_type::now();
    const auto templates = compile_templates(checkout);
    r.compile_ms = elapsed_ms(start);
    r.templates = templates.size();
    if (oracle.empty())
        render_models(r, templates, checkout, table, repetitions, out);
    else
        render_oracle(r, templates, oracle, repetitions, out);
    return r;
}

void write_render_report(std::ostream& os, const render_result& r) {
    os << "templates: " << r.templates << " compiled in " << r.compile_ms
       << " ms\n";
    os << "contexts: " << r.contexts << " in " << r.setup_ms << " ms\n";
    os << "outputs: " << r.outputs << ", bytes: " << r.bytes << "\n";
    auto sorted = r.render_ms;
    std::ranges::sort(sorted);
    if (!sorted.empty()) {
        os << "render ms: min " << sorted.front() << ", median "
           << sorted[sorted.size() / 2] << ", max " << sorted.back()
           << " over " << sorted.size() << " runs\n";
    }
    for (const auto& m : r.messages)
        os << "  " << m << "\n";
}

}

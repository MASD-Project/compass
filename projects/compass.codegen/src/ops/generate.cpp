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
#include "compass.codegen/ops/generate.hpp"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <set>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include "compass.codec/ops/parallel_for.hpp"
#include "compass.codegen/ops/compile.hpp"
#include "compass.codegen/ops/context.hpp"
#include "compass.codegen/ops/entity_context.hpp"
#include "compass.codegen/ops/entity_registry.hpp"
#include "compass.codegen/ops/facts.hpp"
#include "compass.codegen/ops/manifest.hpp"
#include "compass.codegen/ops/model_contexts.hpp"
#include "compass.codegen/ops/output.hpp"
#include "compass.codegen/ops/physical_space.hpp"
#include "compass.codegen/ops/template_context.hpp"
#include "compass.codegen/ops/values.hpp"
#include "compass.model/ops/builder.hpp"
#include "compass.model/ops/entity_claim.hpp"
#include "compass.model/ops/outline.hpp"

namespace compass::codegen::ops {

namespace {

namespace mops = compass::model::ops;
using compass::model::types::model_tables;
using compass::model::types::model_type;
using compass::model::types::value_id;

std::string read_or_throw(const checkout_reader& reader,
    std::string_view path) {
    auto bytes = reader.read(path);
    if (!bytes)
        throw std::runtime_error("codegen: cannot read " + std::string(path));
    return std::move(*bytes);
}

std::string_view file_name(std::string_view path) {
    return path.substr(path.rfind('/') + 1);
}

std::string_view directory_of(std::string_view path) {
    const auto slash = path.rfind('/');
    return slash == std::string_view::npos ? std::string_view{}
        : path.substr(0, slash);
}

/**
 * @brief The .org paths of the checkout, listed when first asked for, and
 * the parsed documents among them, each looked up once.
 */
class documents final {
public:
    explicit documents(const checkout_reader& reader) : reader_(reader) {}

    [[nodiscard]] const std::vector<std::string_view>& org() {
        if (!org_)
            org_ = reader_.list("projects/", ".org");
        return *org_;
    }

    [[nodiscard]] std::optional<mops::document> get(std::string_view path) {
        const auto it = found_.find(std::string(path));
        if (it != found_.end())
            return it->second;
        auto d = reader_.find(path);
        found_.emplace(std::string(path), d);
        return d;
    }

    [[nodiscard]] const mops::document_finder& finder() const noexcept {
        return reader_.find;
    }

    [[nodiscard]] mops::document_lister lister() const {
        return [&reader = reader_](std::string_view prefix) {
            return reader.list(prefix, {});
        };
    }

private:
    const checkout_reader& reader_;
    std::optional<std::vector<std::string_view>> org_;
    std::unordered_map<std::string, std::optional<mops::document>> found_;
};

/**
 * @brief Whether @p path is an .org file of a modeling/ directory under
 * projects/, where ores.codegen looks for models.
 */
bool is_model_candidate(std::string_view path) {
    return path.starts_with("projects/") && path.ends_with(".org") &&
        directory_of(path).ends_with("/modeling");
}

/**
 * @brief The physical-space graph of the library's ores.*.org nodes, read
 * in file-name order as load_graph reads them, with the name and the
 * bytes of each node appended to @p read.
 */
types::physical_space load_graph(const checkout_reader& reader,
    const std::string& templates, std::string& read) {
    std::vector<std::string> names;
    std::vector<std::string> texts;
    for (const auto path : reader.list(templates + "/ores.", ".org")) {
        if (path.find('/', templates.size() + 1) != std::string_view::npos)
            continue;
        names.emplace_back(file_name(path));
        texts.push_back(read_or_throw(reader, path));
    }
    std::vector<node_document> nodes;
    for (std::size_t i = 0; i < names.size(); ++i) {
        nodes.push_back({names[i], texts[i]});
        read += names[i];
        read.push_back('\0');
        read += texts[i];
        read.push_back('\0');
    }
    return load_physical_space(nodes);
}

/**
 * @brief The registry of the checkout's models resolve_targets consults,
 * read from every .org file under a modeling/ directory of projects/.
 */
types::checkout_registry load_registry(documents& docs) {
    std::vector<checkout_file> files;
    for (const auto path : docs.org()) {
        if (path.find("/modeling/") == std::string_view::npos)
            continue;
        if (const auto d = docs.get(path))
            files.push_back({path, d->source});
    }
    return read_checkout_registry(files);
}

/**
 * @brief The registry of the entity models a domain entity's enrichment
 * reads beside its own: those of projects/<component>/modeling/, scanned
 * in path order as _entity_org_by_table scans them, a later claim of a
 * table winning, each loaded into tables of its own; the routes of the
 * trade type catalogues there; the convention families of the modeling
 * directory @p conventions; and the custom-type registry. header_exists
 * is left for each model's generation to set, since it reads the
 * checkout.
 */
types::entity_registry load_entity_registry(documents& docs,
    std::string_view conventions, mops::load_cache& cache) {
    using parts = std::pair<std::string_view, std::string_view>;
    std::vector<std::pair<parts, std::string_view>> paths;
    constexpr std::string_view projects = "projects/";
    constexpr std::string_view modeling = "/modeling/";
    for (const auto path : docs.org()) {
        const auto rest = path.substr(projects.size());
        const auto at = rest.find(modeling);
        if (at == std::string_view::npos || at == 0 ||
            rest.find('/', at + modeling.size()) != std::string_view::npos ||
            rest.substr(0, at).find('/') != std::string_view::npos)
            continue;
        paths.push_back({{rest.substr(0, at),
            rest.substr(at + modeling.size())}, path});
    }
    std::ranges::sort(paths);

    const auto& find = docs.finder();
    types::entity_registry registry;
    registry.custom_types = mops::custom_type_headers(find, cache);
    model_tables scratch;
    mops::builder b(scratch);
    mops::outline outline;
    for (const auto& [key, path] : paths) {
        const auto doc = docs.get(path);
        if (!doc)
            continue;
        const auto type = mops::model_type_of(mops::declared_type(*doc));
        if (type == model_type::trade_type_catalogue) {
            mops::build_outline(doc->source, *doc->tables, outline);
            registry.trade_type_routes[
                std::filesystem::path(key.second).stem().string()] =
                trade_type_routes_of(outline);
        }
        const auto claim = mops::entity_claim_of(doc->source);
        if (!claim)
            continue;
        auto& owner = registry.tables[claim->table];
        owner = {claim->entity_singular, std::nullopt};
        if (!type)
            continue;
        std::string error;
        const auto id = mops::load_model(b, *doc, *type, find, docs.lister(),
            cache, &error);
        if (error.empty())
            owner.parent = parent_entity_of(scratch, scratch.models.roots[id]);
    }

    for (const auto path : docs.org()) {
        if (path.find(modeling) == std::string_view::npos)
            continue;
        if (const auto doc = docs.get(path))
            add_directory_model(
                registry.directories[std::string(directory_of(path))],
                doc->source);
    }

    for (const auto path : docs.org()) {
        if (directory_of(path) != conventions)
            continue;
        if (const auto doc = docs.get(path)) {
            if (auto f = convention_family_of(path, doc->source))
                registry.convention_families.edit().push_back(
                    std::move(*f));
        }
    }
    std::ranges::sort(registry.convention_families.edit(), {},
        &types::convention_family::key);

    constexpr std::string_view group_suffix = "_field_group.org";
    for (const auto path : docs.org()) {
        if (!path.ends_with(group_suffix) ||
            path.find(modeling) == std::string_view::npos)
            continue;
        const auto doc = docs.get(path);
        if (!doc)
            continue;
        std::string error;
        const auto id = mops::load_model(b, *doc, model_type::field_group,
            find, docs.lister(), cache, &error);
        if (!error.empty())
            continue;
        const auto& t = b.tables();
        const auto group = values::member(t, scratch.models.roots[id],
            "field_group");
        if (!values::truthy(t, group, "has_economic_fields"))
            continue;
        const auto name = file_name(path);
        registry.economic_field_groups.edit().emplace(
            name.substr(0, name.size() - group_suffix.size()));
    }
    return registry;
}

/**
 * @brief A model's drawer properties followed by its
 * physical_space_overrides, a later key winning. The properties view
 * the outline they were read from and the overrides held here.
 */
struct model_properties {
    std::vector<std::pair<std::string, bool>> overrides;
    std::vector<drawer_property> properties;
};

model_properties drawer_properties(const mops::document& doc,
    mops::outline& outline, const mops::document_finder& find,
    mops::load_cache& cache) {
    mops::build_outline(doc.source, *doc.tables, outline);
    model_properties out;
    out.overrides = mops::physical_space_overrides(outline, find, cache);
    for (std::size_t i = 0; i < outline.file_property_keys.size(); ++i)
        out.properties.push_back({outline.file_property_keys[i],
            outline.file_property_values[i]});
    for (const auto& [key, enabled] : out.overrides)
        out.properties.push_back({key, enabled ? "true" : "false"});
    return out;
}

/**
 * @brief _shell_command_units for every modeling directory of projects/:
 * the singulars of the entity and junction models in it, or one level
 * below it, that enable ores.cpp.shell-command.
 */
std::map<std::string, std::set<std::string>, std::less<>> shell_units(
    documents& docs, mops::load_cache& cache) {
    constexpr std::string_view facet = "ores.cpp.shell-command";
    constexpr std::string_view addresses[] = {facet};
    const auto& find = docs.finder();
    model_tables scratch;
    mops::builder b(scratch);
    mops::outline outline;
    std::map<std::string, std::set<std::string>, std::less<>> out;
    for (const auto path : docs.org()) {
        if (path.find("/modeling/") == std::string_view::npos)
            continue;
        const auto doc = docs.get(path);
        if (!doc)
            continue;
        const auto type = mops::model_type_of(mops::declared_type(*doc));
        if (type != model_type::entity && type != model_type::junction)
            continue;
        if (!facet_enabled(drawer_properties(*doc, outline, find, cache)
                .properties, facet, addresses, false))
            continue;
        std::string error;
        const auto id = mops::load_model(b, *doc, *type, find, docs.lister(),
            cache, &error);
        if (!error.empty())
            continue;
        const auto& t = b.tables();
        const auto body = values::member(t, t.models.roots[id],
            *type == model_type::entity ? "domain_entity" : "junction");
        const auto name = values::str(t, body,
            *type == model_type::entity ? "entity_singular" : "name_singular");
        if (name.empty())
            continue;
        const std::filesystem::path file(path);
        out[file.parent_path().string()].emplace(name);
        out[file.parent_path().parent_path().string()].emplace(name);
    }
    return out;
}

/**
 * @brief The model types compass.codegen builds whole contexts for, with
 * their ores.codegen names.
 */
struct context_type {
    model_type type;
    std::string_view name;
};

constexpr context_type context_types[] = {
    {model_type::entity, "domain_entity"},
    {model_type::component, "component"},
    {model_type::junction, "junction"},
    {model_type::operation, "operation"},
    {model_type::field_group, "field_group"},
    {model_type::family, "family"},
    {model_type::asset_class_catalogue, "asset_class_catalogue"},
    {model_type::outcome_catalogue, "outcome_catalogue"},
    {model_type::trade_type_catalogue, "trade_type_catalogue"}};

/**
 * @brief A model's shared context and what its template variants are
 * built from: a junction's surface shape or a family's service context.
 */
struct built_context {
    value_id context;
    value_id prepared;
};

built_context build_context(mops::builder& b, model_type type,
    value_id library, value_id root, std::string_view stem,
    std::string_view type_name, const types::entity_registry& entities,
    const types::entity_site& site, const family_members& members,
    const field_group_models& groups) {
    switch (type) {
    case model_type::entity:
        return {entity_context(b, library, root, stem, entities, site),
            value_id::none()};
    case model_type::junction: {
        const auto [context, surface] = junction_context(b, library, root,
            stem, entities, site);
        return {context, surface};
    }
    case model_type::family: {
        const auto [context, service] = family_context(b, library, root,
            stem, entities, site, members);
        return {context, service};
    }
    case model_type::operation:
        return {operation_context(b, library, root, stem, entities, site),
            value_id::none()};
    case model_type::field_group:
        return {field_group_context(b, library, root, stem, entities, site),
            value_id::none()};
    case model_type::trade_type_catalogue:
        return {trade_type_catalogue_context(b, library, root, stem,
            entities, site, groups), value_id::none()};
    default:
        return {model_context(b, library, root, stem, type_name),
            value_id::none()};
    }
}

/**
 * @brief The entity models beside the family at @p path, by the singular
 * each declares, the first in path order winning, loaded into @p b.
 */
family_members family_members_of(std::string_view path, documents& docs,
    mops::builder& b, mops::load_cache& cache) {
    const auto dir = path.substr(0, path.rfind('/') + 1);
    family_members out;
    for (const auto p : docs.org()) {
        if (!p.starts_with(dir) ||
            p.find('/', dir.size()) != std::string_view::npos)
            continue;
        const auto d = docs.get(p);
        if (!d || mops::model_type_of(mops::declared_type(*d)) !=
                model_type::entity)
            continue;
        std::string error;
        const auto id = mops::load_model(b, *d, model_type::entity,
            docs.finder(), docs.lister(), cache, &error);
        if (!error.empty())
            continue;
        const auto entity = values::member(b.tables(),
            b.tables().models.roots[id], "domain_entity");
        const std::string singular(values::str(b.tables(), entity,
            "entity_singular"));
        if (!singular.empty())
            out.try_emplace(singular, entity);
    }
    return out;
}

/**
 * @brief The field group models beside the model at @p path, by the
 * dotted name of their file (<name>_field_group.org), loaded into @p b.
 */
field_group_models field_group_models_of(std::string_view path,
    documents& docs, mops::builder& b, mops::load_cache& cache) {
    constexpr std::string_view suffix = "_field_group.org";
    const auto dir = path.substr(0, path.rfind('/') + 1);
    field_group_models out;
    for (const auto p : docs.org()) {
        if (!p.starts_with(dir) || !p.ends_with(suffix) ||
            p.find('/', dir.size()) != std::string_view::npos)
            continue;
        const auto d = docs.get(p);
        if (!d)
            continue;
        std::string error;
        const auto id = mops::load_model(b, *d, model_type::field_group,
            docs.finder(), docs.lister(), cache, &error);
        if (!error.empty())
            continue;
        const auto name = p.substr(dir.size());
        out.try_emplace(std::string(name.substr(0, name.size() -
            suffix.size())), values::member(b.tables(),
                b.tables().models.roots[id], "field_group"));
    }
    return out;
}

/**
 * @brief The modeling directory the catalogue gives @p component.
 */
std::string_view modeling_dir_of(
    const std::vector<catalogue_component>& catalogue,
    std::string_view component) {
    const auto it = std::ranges::find(catalogue, component,
        &catalogue_component::name);
    if (it == catalogue.end())
        throw std::runtime_error("codegen: the component catalogue has no " +
            std::string(component));
    std::string_view dir = it->modeling_dir;
    while (dir.ends_with('/'))
        dir.remove_suffix(1);
    return dir;
}

}

namespace {

/**
 * @brief One output of a model: what to render it from, or kept when it
 * is to stay as the model's last generation left it, with no context.
 */
struct model_row {
    planned_output output;
    bool kept = false;
};

/**
 * @brief The outputs of one model, each path once, in the order its
 * units first name them; context says whether a context was built, and
 * error why the model did not load.
 */
struct model_plan {
    std::vector<model_row> rows;
    bool context = false;
    std::string error;
};

/**
 * @brief Whether the outputs of a template are to be rendered; an empty
 * function renders every template.
 */
using template_filter = std::function<bool(std::string_view name)>;

/**
 * @brief Plans the outputs of the model at @p path with the values of
 * @p b, reading @p registry and @p entities, which record what is read
 * of them. A unit whose template @p wanted refuses is planned as kept.
 */
model_plan plan_model(mops::builder& b, value_id library,
    const generation& g, const types::checkout_registry& registry,
    const types::entity_registry& entities, std::string_view path,
    const checkout_reader& reader, mops::load_cache& cache,
    const template_filter& wanted) {
    model_plan out;
    documents docs(reader);
    const auto doc = docs.get(path);
    if (!doc)
        return out;
    const auto type = mops::model_type_of(mops::declared_type(*doc));
    const auto known = std::ranges::find_if(context_types,
        [&](const context_type& c) { return type && c.type == *type; });
    if (known == std::end(context_types))
        return out;
    const auto type_name = known->name;
    const auto& find = docs.finder();
    const auto id = mops::load_model(b, *doc, *type, find, docs.lister(),
        cache, &out.error);
    if (!out.error.empty()) {
        out.error = std::string(path) + ": " + out.error;
        return out;
    }
    const auto root = b.tables().models.roots[id];
    mops::outline outline;
    const auto read = drawer_properties(*doc, outline, find, cache);
    const auto& properties = read.properties;
    const auto units = model_units(g.graph, registry, type_name, b.tables(),
        root, path, properties);
    if (units.empty())
        return out;
    const auto stem = std::filesystem::path(path).stem().string();
    constexpr std::string_view wire_codec_addresses[] = {
        "ores.cpp.wire-codec.codec_header",
        "ores.cpp.wire-codec.codec_impl",
        "ores.cpp.wire-codec.protocol_codec_impl"};
    constexpr std::string_view protocol_addresses[] = {"ores.cpp.protocol"};
    const types::entity_site site{std::string(path),
        facet_enabled(properties, "ores.cpp.wire-codec",
            wire_codec_addresses, false),
        facet_enabled(properties, "ores.cpp.protocol", protocol_addresses,
            true),
        protocol_owned(registry, b.tables(),
            values::member(b.tables(), root, type_name), path)};
    const auto members = *type == model_type::family
        ? family_members_of(path, docs, b, cache) : family_members{};
    const auto groups = *type == model_type::trade_type_catalogue
        ? field_group_models_of(path, docs, b, cache)
        : field_group_models{};
    const auto [context, prepared] = build_context(b, *type, library,
        root, stem, type_name, entities, site, members, groups);
    out.context = true;
    std::unordered_map<std::string, std::size_t> rows;
    for (const auto& u : units) {
        model_row row{{u.output, u.template_name, value_id::none(),
            type_name}, wanted && !wanted(u.template_name)};
        if (!row.kept) {
            const auto variant = template_context(b, context, stem,
                type_name, u.template_name, entities, site, prepared);
            row.output.context = variant.is_none() ? context : variant;
        }
        const auto [at, added] = rows.try_emplace(u.output,
            out.rows.size());
        if (added)
            out.rows.push_back(std::move(row));
        else
            out.rows[at->second] = std::move(row);
    }
    return out;
}

/**
 * @brief Compiles the templates of the library @p s names, adding the
 * fact of each to @p facts when given.
 */
template_map read_templates(const checkout_reader& reader,
    const generation_settings& s, std::vector<fact>* facts) {
    const auto dir = s.library + "/templates/";
    template_map templates;
    for (const auto path : reader.list(dir, ".mustache")) {
        if (path.find('/', dir.size()) != std::string_view::npos)
            continue;
        const auto bytes = read_or_throw(reader, path);
        if (facts != nullptr)
            facts->push_back(fact_of(template_fact(file_name(path)), bytes));
        templates.emplace(std::string(file_name(path)), compile(bytes));
    }
    return templates;
}

const types::compiled_template& template_of(const template_map& templates,
    const planned_output& output) {
    const auto it = templates.find(output.template_name);
    if (it == templates.end())
        throw std::runtime_error("codegen: " + output.output +
            " needs the template " + output.template_name +
            ", which the library does not have");
    return it->second;
}

/**
 * @brief The templates among the inputs of a model that changed since its
 * last generation, when nothing else changed; nothing when there is no
 * last generation or another input changed, so every output is rendered.
 */
std::optional<std::set<std::string_view>> changed_templates(
    const compass::codec::types::part_sink& out) {
    const auto changed = out.changed();
    if (!changed)
        return std::nullopt;
    const auto prefix = template_fact({});
    std::set<std::string_view> names;
    for (const auto& c : *changed) {
        if (c.kind != compass::codec::types::input_kind::fact ||
            !c.key.starts_with(prefix))
            return std::nullopt;
        names.insert(c.key.substr(prefix.size()));
    }
    return names;
}

}

std::string template_fact(std::string_view name) {
    return types::fact_name("template", name);
}

template_map compile_templates(const checkout_reader& reader,
    const generation_settings& s) {
    return read_templates(reader, s, nullptr);
}

generation prepare_generation(const checkout_reader& reader,
    const generation_settings& s) {
    generation g;
    g.settings = s;
    g.licence = read_or_throw(reader, s.library + "/data/licence-GPL-v3.txt");
    g.modeline = read_or_throw(reader, s.library + "/data/modeline.org");
    g.facts.push_back(fact_of(std::string(library_fact), g.licence +
        std::string(1, '\0') + g.modeline + std::string(1, '\0') +
        std::to_string(s.year)));
    std::string nodes;
    g.graph = load_graph(reader, s.library + "/templates", nodes);
    g.facts.push_back(fact_of(std::string(graph_fact), nodes));
    documents docs(reader);
    g.registry = load_registry(docs);

    const auto catalogue = read_component_catalogue(
        read_or_throw(reader, s.library + "/component_catalogue.org"));
    const auto regenerated = regenerated_models(catalogue, docs.org(),
        [&](std::string_view path) {
            const auto d = is_model_candidate(path) ? docs.get(path)
                : std::nullopt;
            return d ? std::string(mops::declared_type(*d)) : std::string();
        });

    mops::load_cache cache;
    g.entities = load_entity_registry(docs,
        modeling_dir_of(catalogue, s.convention_component), cache);
    g.entities.shell_units = shell_units(docs, cache);
    g.entities.shell_local_units = g.registry.shell_registrars.unrecorded();
    for (const auto path : docs.org()) {
        if (!regenerated.contains(path))
            continue;
        const auto doc = docs.get(path);
        if (!doc)
            continue;
        const auto type = mops::model_type_of(mops::declared_type(*doc));
        if (std::ranges::any_of(context_types, [&](const context_type& c) {
                return type && c.type == *type;
            }))
            g.models.emplace_back(path);
    }
    g.templates = read_templates(reader, s, &g.facts);
    publish_facts(g.facts, g.entities);
    publish_facts(g.facts, g.registry);
    return g;
}

void plan_generation(generation_plan& plan, const checkout_reader& reader,
    const generation_settings& s) {
    const auto g = prepare_generation(reader, s);
    mops::builder b(plan.tables);
    const auto library = library_values(b, g.licence, g.modeline, s.year);
    auto entities = g.entities;
    entities.header_exists = reader.exists;
    mops::load_cache cache;
    std::unordered_map<std::string, std::size_t> rows;
    for (const auto& model : g.models) {
        auto m = plan_model(b, library, g, g.registry, entities, model,
            reader, cache, {});
        if (!m.error.empty()) {
            plan.messages.push_back(std::move(m.error));
            continue;
        }
        if (m.context)
            ++plan.contexts;
        for (auto& row : m.rows) {
            const auto [at, added] = rows.try_emplace(row.output.output,
                plan.outputs.size());
            if (added)
                plan.outputs.push_back(std::move(row.output));
            else
                plan.outputs[at->second] = std::move(row.output);
        }
    }
}

void render_planned(std::string& text, const template_map& templates,
    const generation_plan& plan, const planned_output& output) {
    render_output(text, output.template_name, template_of(templates, output),
        plan.tables, output.context);
}

void generate_model(const generation& g, std::string_view model,
    const checkout_reader& reader, mops::load_cache& cache,
    compass::codec::types::part_sink& out) {
    types::fact_reads reads;
    auto entities = g.entities;
    entities.record_to(&reads);
    entities.header_exists = reader.exists;
    auto registry = g.registry;
    registry.record_to(&reads);
    out.read(library_fact);
    out.read(graph_fact);

    const auto again = changed_templates(out);
    const template_filter wanted = again
        ? template_filter([&](std::string_view name) {
              return again->contains(name);
          })
        : template_filter{};
    generation_plan plan;
    mops::builder b(plan.tables);
    const auto library = library_values(b, g.licence, g.modeline,
        g.settings.year);
    const auto planned = plan_model(b, library, g, registry, entities, model,
        reader, cache, wanted);
    if (!planned.error.empty())
        throw std::runtime_error("codegen: a model did not load: " +
            planned.error);
    std::string text;
    for (const auto& row : planned.rows) {
        out.read(template_fact(row.output.template_name));
        if (row.kept) {
            if (!out.keep(row.output.output))
                throw std::runtime_error("codegen: the last generation of " +
                    std::string(model) + " did not write " +
                    row.output.output);
            continue;
        }
        text.clear();
        render_output(text, row.output.template_name,
            template_of(g.templates, row.output), plan.tables,
            row.output.context);
        out.add(row.output.output, text);
    }
    for (const auto& name : reads.names())
        out.read(name);
}

}

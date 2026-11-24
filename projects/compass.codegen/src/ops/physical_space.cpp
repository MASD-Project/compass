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
#include "compass.codegen/ops/physical_space.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

namespace {

using compass::model::types::model_tables;
using compass::model::types::value_id;

// The addresses of these lists are under the graph's root, which is the
// project's product, and are spelled without it.
constexpr std::string_view protocol_dependent_facets[] = {
    "cpp.http-route", "doc.http-recipe", "cpp.nats-eventing",
    "cpp.nats-event-registrar", "cpp.eventing-integration-test",
    "cpp.shell-command", "doc.shell-recipe"};

/**
 * @brief _PROTOCOL_FACETS: what an operation model that owns an entity's
 * protocol takes from the entity.
 */
constexpr std::string_view protocol_facets[] = {
    "cpp.protocol", "ts.protocol", "cpp.nats-eventing",
    "cpp.nats-event-registrar", "cpp.eventing-integration-test",
    "cpp.shell-command", "doc.shell-recipe", "cpp.http-route",
    "doc.http-recipe"};

/**
 * @brief _NO_FLAT_AUDIT_HISTORY_PROVIDER_FACETS: what needs version rows
 * with a flat actor.
 */
constexpr std::string_view no_flat_audit_facets[] = {
    "cpp.history-provider-registrar"};

/**
 * @brief _STAGING_ONLY_FACETS: the menu a staging shim must not render
 * when another component models its plural.
 */
constexpr std::string_view staging_only_facets[] = {
    "cpp.shell-command", "doc.shell-recipe"};

/**
 * @brief _STAGING_ONLY_EVENTING_FACETS: what a model with no notify
 * trigger cannot announce.
 */
constexpr std::string_view staging_only_eventing_facets[] = {
    "cpp.nats-eventing", "cpp.nats-event-registrar",
    "cpp.eventing-integration-test"};

/**
 * @brief _NO_TEMPORAL_HISTORY_ARCHETYPES: what a current-state entity,
 * which has no version rows, cannot build.
 */
constexpr std::string_view no_temporal_archetypes[] = {
    "cpp.presentation.history_field_mapper_header",
    "cpp.presentation.history_field_mapper_impl",
    "cpp.eventing-integration-test.nats_integration_test"};

/**
 * @brief _JUNCTION_MESSAGING_FACETS: what serves a junction's list reads,
 * which one with no :list_by: side does not have.
 */
constexpr std::string_view junction_messaging_facets[] = {
    "cpp.nats-handler", "cpp.nats-sub-registrar",
    "cpp.protocol", "cpp.service", "ts.protocol"};

/**
 * @brief _JUNCTION_HISTORY_FACETS: what reads the history of a junction
 * that states :history: true.
 */
constexpr std::string_view junction_history_facets[] = {
    "cpp.history-provider-registrar", "cpp.presentation"};

/**
 * @brief _JUNCTION_EVENT_FACETS and _JUNCTION_EVENT_ARCHETYPES: what
 * announces the changes of a junction that states :events: true.
 */
constexpr std::string_view junction_event_facets[] = {
    "cpp.nats-eventing", "cpp.nats-event-registrar",
    "sql.schema.notify_trigger", "sql.schema.notify_trigger_drop"};

constexpr std::string_view junction_event_archetypes[] = {
    "sql.schema.notify_trigger", "sql.schema.notify_trigger_drop"};

constexpr std::string_view shell_aggregator_templates[] = {
    "cpp_shell_command_aggregator_header.hpp.mustache",
    "cpp_shell_command_aggregator_impl.cpp.mustache"};

bool is_space(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

std::string_view strip(std::string_view s) {
    while (!s.empty() && is_space(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && is_space(s.back()))
        s.remove_suffix(1);
    return s;
}

std::string lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::vector<std::string> words(std::string_view s) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && is_space(s[i]))
            ++i;
        const auto first = i;
        while (i < s.size() && !is_space(s[i]))
            ++i;
        if (i > first)
            out.emplace_back(s.substr(first, i - first));
    }
    return out;
}

bool is_word(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

/**
 * @brief _parse_default: unset or blank is @p fallback, and any of the
 * disabled words, in any case, is false.
 */
bool parse_default(const std::optional<std::string>& value, bool fallback) {
    if (!value || strip(*value).empty())
        return fallback;
    const auto v = lower(strip(*value));
    return v != "disabled" && v != "off" && v != "false" && v != "no" &&
        v != "0";
}

/**
 * @brief The #+key: value lines of a node before its first heading, keys
 * in lower case, a later line winning.
 */
std::map<std::string, std::string> keywords(std::string_view text) {
    std::map<std::string, std::string> kw;
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        auto line = text.substr(start, end - start);
        start = end + 1;
        if (line.starts_with("* "))
            break;
        if (!line.starts_with("#+") || line.size() < 3 || !is_word(line[2]))
            continue;
        std::size_t k = 3;
        while (k < line.size() && (is_word(line[k]) || line[k] == '-'))
            ++k;
        if (k >= line.size() || line[k] != ':')
            continue;
        kw[lower(line.substr(2, k - 2))] =
            std::string(strip(line.substr(k + 1)));
    }
    return kw;
}

/**
 * @brief The first :tangle <name>.mustache of the node, or empty.
 */
std::string tangled_template(std::string_view text) {
    constexpr std::string_view tangle = ":tangle";
    for (auto p = text.find(tangle); p != std::string_view::npos;
         p = text.find(tangle, p + 1)) {
        auto q = p + tangle.size();
        const auto spaces = q;
        while (q < text.size() && is_space(text[q]))
            ++q;
        if (q == spaces)
            continue;
        auto e = q;
        while (e < text.size() && !is_space(text[e]))
            ++e;
        const auto word = text.substr(q, e - q);
        const auto at = word.rfind(".mustache");
        if (at != std::string_view::npos && at > 0)
            return std::string(word.substr(0, at + 9));
    }
    return {};
}

std::optional<std::string> get(const std::map<std::string, std::string>& kw,
    const std::string& key) {
    const auto it = kw.find(key);
    if (it == kw.end())
        return std::nullopt;
    return it->second;
}

std::string get_or(const std::map<std::string, std::string>& kw,
    const std::string& key, std::string_view fallback = {}) {
    const auto v = get(kw, key);
    return v ? *v : std::string(fallback);
}

/**
 * @brief Python's object.get(key) of a string member: none when absent.
 */
std::optional<std::string_view> string_member(const model_tables& t,
    value_id object, std::string_view key) {
    const auto v = values::member(t, object, key);
    if (v.is_none())
        return std::nullopt;
    return values::str(t, v);
}

std::string get_or(const model_tables& t, value_id object,
    std::string_view key, std::string_view fallback) {
    return std::string(string_member(t, object, key).value_or(fallback));
}

using overrides = std::map<std::string, bool, std::less<>>;

/**
 * @brief _enabled_overrides: each <root>[.part]*.enabled key of the drawer.
 */
overrides enabled_overrides(std::span<const drawer_property> properties,
    std::string_view root) {
    overrides out;
    constexpr std::string_view suffix = ".enabled";
    for (const auto& p : properties) {
        const auto key = strip(p.key);
        if (!key.ends_with(suffix))
            continue;
        const auto address = key.substr(0, key.size() - suffix.size());
        if (address != root && !(address.starts_with(root) &&
                address.size() > root.size() && address[root.size()] == '.'))
            continue;
        bool valid = true;
        for (std::size_t i = root.size(); i < address.size(); ++i) {
            if (address[i] == '.') {
                valid = valid && i + 1 < address.size() &&
                    address[i + 1] != '.';
            } else if (!is_word(address[i]) && address[i] != '-') {
                valid = false;
            }
        }
        if (!valid)
            continue;
        const auto v = lower(strip(p.value));
        out[std::string(address)] = v == "true" || v == "t" || v == "yes" ||
            v == "1";
    }
    return out;
}

bool is_enabled(std::string_view address, std::string_view facet,
    std::string_view ts, std::string_view root, const overrides& o,
    bool fallback) {
    for (const auto key : {address, facet, ts, root}) {
        if (key.empty())
            continue;
        const auto it = o.find(key);
        if (it != o.end())
            return it->second;
    }
    return fallback;
}

bool serves(const std::vector<std::string>& model_types,
    std::string_view type) {
    return model_types.empty() ||
        std::ranges::find(model_types, type) != model_types.end();
}

std::optional<std::size_t> facet_row(const types::physical_space& g,
    std::string_view address) {
    const auto& a = g.facets.addresses;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i] == address)
            return i;
    return std::nullopt;
}

/**
 * @brief _facet_renders: whether @p facet serves the model type and is
 * enabled for this model.
 */
bool facet_renders(const types::physical_space& g, std::string_view facet,
    std::string_view type, const overrides& o) {
    const auto row = facet_row(g, facet);
    if (row && !serves(g.facets.model_types[*row], type))
        return false;
    return is_enabled(facet, facet,
        row ? std::string_view(g.facets.technical_spaces[*row]) : "",
        g.root, o, row ? g.facets.defaults[*row] != 0 : true);
}

void replace_all(std::string& s, std::string_view from, std::string_view to) {
    for (auto p = s.find(from); p != std::string::npos;
         p = s.find(from, p + to.size()))
        s.replace(p, from.size(), to);
}

using facet_set = std::set<std::string, std::less<>>;

/**
 * @brief The address @p relative names under the root @p root.
 */
std::string under(std::string_view root, std::string_view relative) {
    return std::string(root) + "." + std::string(relative);
}

void drop(facet_set& facets, std::string_view root,
    std::span<const std::string_view> dropped) {
    for (const auto d : dropped) {
        const auto it = facets.find(under(root, d));
        if (it != facets.end())
            facets.erase(it);
    }
}

std::string_view parent(std::string_view path) {
    const auto slash = path.rfind('/');
    return slash == std::string_view::npos ? "" : path.substr(0, slash);
}

std::string_view file_name(std::string_view path) {
    const auto slash = path.rfind('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

/**
 * @brief Whether @p profile is one of the comma-separated @p profiles.
 */
bool has_profile(std::string_view profiles, std::string_view profile) {
    for (;;) {
        const auto comma = profiles.find(',');
        if (strip(profiles.substr(0, comma)) == profile)
            return true;
        if (comma == std::string_view::npos)
            return false;
        profiles.remove_prefix(comma + 1);
    }
}

/**
 * @brief The #+type: of a document, as _ORG_TYPE_RE finds it in its first
 * 4096 bytes: the first such line holding one word, the key in any case.
 */
std::string_view org_type(std::string_view text) {
    text = text.substr(0, 4096);
    constexpr std::string_view key = "#+type:";
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        const auto line = text.substr(start, end - start);
        start = end + 1;
        if (line.size() < key.size() ||
            lower(line.substr(0, key.size())) != key)
            continue;
        const auto value = strip(line.substr(key.size()));
        if (!value.empty() && std::ranges::none_of(value, is_space))
            return value;
    }
    return {};
}

/**
 * @brief The plural _plural_owners reads from a document: the value of
 * its first #+entity_plural: or #+name: line that states one.
 */
std::string_view stated_plural(std::string_view text) {
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        const auto line = text.substr(start, end - start);
        start = end + 1;
        if (!line.starts_with("#+"))
            continue;
        const auto colon = line.find(':');
        const auto key = strip(line.substr(2, colon == std::string_view::npos
            ? std::string_view::npos : colon - 2));
        const auto value = colon == std::string_view::npos
            ? std::string_view() : strip(line.substr(colon + 1));
        if ((key == "entity_plural" || key == "name") && !value.empty())
            return value;
    }
    return {};
}

/**
 * @brief snake_to_pascal: each underscore-separated word capitalised as
 * str.capitalize does it.
 */
std::string snake_to_pascal(std::string_view snake) {
    std::string out;
    bool first = true;
    for (const auto c : snake) {
        if (c == '_') {
            first = true;
            continue;
        }
        const auto u = static_cast<unsigned char>(c);
        out += static_cast<char>(first ? std::toupper(u) : std::tolower(u));
        first = false;
    }
    return out;
}

/**
 * @brief A placeholder of an output pattern, braces included, and its
 * value; none when the model lacks what the placeholder needs, so a
 * pattern naming it does not resolve.
 */
struct placeholder {
    std::string name;
    std::optional<std::string> value;
};

/**
 * @brief @p pattern with each placeholder replaced in turn, or none when
 * it names one with no value.
 */
std::optional<std::string> resolve(std::string pattern,
    std::span<const placeholder> placeholders) {
    for (const auto& p : placeholders) {
        if (pattern.find(p.name) == std::string::npos)
            continue;
        if (!p.value)
            return std::nullopt;
        replace_all(pattern, p.name, *p.value);
    }
    return pattern;
}

/**
 * @brief resolve_output_path's placeholders for a component, whose
 * directory under projects/ is the one above the modeling/ of
 * @p model_path.
 */
std::vector<placeholder> component_placeholders(const model_tables& t,
    value_id component, std::optional<std::string_view> kind,
    std::string_view model_path, std::string_view product) {
    const auto name = get_or(t, component, "name", "unknown");
    const auto full_name = get_or(t, component, "full_name",
        under(product, name));
    std::string_view dir = parent(parent(model_path));
    if (dir.starts_with("projects/"))
        dir.remove_prefix(9);
    const std::string component_dir(dir);
    const auto include_root = kind && *kind == "adapter" &&
        component_dir.find('/') != std::string::npos
        ? component_dir.substr(0, component_dir.find('/')) : full_name;
    return {{"{component_dir}", component_dir}, {"{component}", name},
        {"{component_full}", full_name},
        {"{component_include_root}", include_root}};
}

/**
 * @brief _component_path_vars of @p model, as placeholders.
 */
std::vector<placeholder> path_placeholders(const model_tables& t,
    value_id model, std::string_view product) {
    const auto component = get_or(t, model, "component", "unknown");
    const auto subcomponent = get_or(t, model, "subcomponent", "");
    const bool split = !subcomponent.empty();
    const auto own_dir = under(product, component);
    const auto cached_by = get_or(t, model, "cached_by", component);
    const auto dot = cached_by.find('.');
    const auto cache_component = cached_by.substr(0, dot);
    const auto cache_subcomponent = dot == std::string::npos
        ? std::string("core") : cached_by.substr(dot + 1);
    return {
        {"{component}", component},
        {"{component_dir}", split ? own_dir + "/" + subcomponent : own_dir},
        {"{component_core_dir}", split ? own_dir + "/core" : own_dir},
        {"{component_service_dir}", split ? own_dir + "/service" : own_dir},
        {"{component_include}", split ? component + "." + subcomponent
            : get_or(t, model, "component_include", component)},
        {"{component_core}", split ? component + ".core"
            : get_or(t, model, "component_core", component)},
        {"{component_service}", split ? component + ".service"
            : get_or(t, model, "component_service", component)},
        {"{cache_component_dir}", under(product, cache_component)},
        {"{cache_component}", cache_component},
        {"{cache_subcomponent}", cache_subcomponent},
        {"{generator_facet_name}",
            get_or(t, model, "generator_facet_name", "generators")}};
}

/**
 * @brief _resolve_flagged_entity's placeholders, named after @p singular
 * and filled only for a model that declares what they need.
 */
void add_flagged_placeholders(std::vector<placeholder>& out,
    const model_tables& t, value_id model, const std::string& singular) {
    const auto stated = [&](std::string_view key) {
        return !strip(values::str(t, model, key)).empty();
    };
    const auto when = [](bool declared, std::string value) {
        return declared ? std::optional(std::move(value)) : std::nullopt;
    };
    const bool writable = !(values::truthy(t, model, "read_only") ||
        values::truthy(t, model, "client_read_only"));
    out.push_back({"{web_entity}",
        when(stated("web_resource"), singular + "_web")});
    out.push_back({"{web_classification_entity}",
        when(stated("web_classification"), singular + "_classification")});
    out.push_back({"{system_owned_entity}", when(writable &&
        values::truthy(t, model, "system_tenant_validation"),
        singular + "_system_owned")});
}

/**
 * @brief resolve_output_path's placeholders for a domain entity:
 * _component_path_vars, the entity's names, its shell menu and the
 * flagged placeholders.
 */
std::vector<placeholder> entity_placeholders(const model_tables& t,
    value_id entity, std::string_view product) {
    auto out = path_placeholders(t, entity, product);
    const auto singular = get_or(t, entity, "entity_singular", "unknown");
    const auto plural = get_or(t, entity, "entity_plural", singular + "s");
    const auto menu = values::str(t, entity, "entity_plural");
    out.push_back({"{entity_plural}", plural});
    out.push_back({"{entity}", singular});
    out.push_back({"{EntityPascal}", snake_to_pascal(singular)});
    out.push_back({"{shell_menu}", std::string(menu.empty()
        ? values::str(t, entity, "entity_singular") : menu)});
    add_flagged_placeholders(out, t, entity, singular);
    return out;
}

/**
 * @brief resolve_output_path's placeholders for a junction, whose plural
 * is its name.
 */
std::vector<placeholder> junction_placeholders(const model_tables& t,
    value_id junction, std::string_view product) {
    auto out = path_placeholders(t, junction, product);
    const auto name = get_or(t, junction, "name", "unknown");
    const std::string singular(values::str(t, junction, "name_singular"));
    const auto plural = values::str(t, junction, "entity_plural");
    out.push_back({"{junction_name}", name});
    out.push_back({"{entity}", singular});
    out.push_back({"{EntityPascal}", snake_to_pascal(singular)});
    out.push_back({"{entity_plural}", name});
    out.push_back({"{shell_menu}", std::string(plural.empty()
        ? values::str(t, junction, "name") : plural)});
    add_flagged_placeholders(out, t, junction, singular);
    return out;
}

/**
 * @brief resolve_output_path's placeholders for an operation, whose menu
 * and plural are its entity singular.
 */
std::vector<placeholder> operation_placeholders(const model_tables& t,
    value_id operation, std::string_view product) {
    auto out = path_placeholders(t, operation, product);
    const auto singular = get_or(t, operation, "entity_singular", "unknown");
    const std::string menu(values::str(t, operation, "entity_singular"));
    out.push_back({"{entity}", singular});
    out.push_back({"{EntityPascal}", snake_to_pascal(singular)});
    out.push_back({"{entity_plural}", menu});
    out.push_back({"{shell_menu}", menu});
    return out;
}

/**
 * @brief resolve_output_path's placeholders for a family.
 */
std::vector<placeholder> family_placeholders(const model_tables& t,
    value_id family, std::string_view product) {
    auto out = path_placeholders(t, family, product);
    const auto singular = get_or(t, family, "entity_singular", "unknown");
    out.push_back({"{entity_plural}",
        get_or(t, family, "entity_plural", singular + "s")});
    out.push_back({"{entity}", singular});
    out.push_back({"{EntityPascal}", snake_to_pascal(singular)});
    return out;
}

/**
 * @brief resolve_output_path's placeholders for a field group.
 */
std::vector<placeholder> field_group_placeholders(const model_tables& t,
    value_id group, std::string_view product) {
    const auto component = get_or(t, group, "component", "unknown");
    const auto subcomponent = get_or(t, group, "subcomponent", "");
    const auto stated_include = values::str(t, group, "component_include");
    const auto include = subcomponent.empty()
        ? get_or(t, group, "component_include", component)
        : stated_include.empty() ? component + "." + subcomponent
        : std::string(stated_include);
    const auto singular = get_or(t, group, "entity_singular", "unknown");
    return {
        {"{component_dir}", subcomponent.empty() ? under(product, component)
            : under(product, component) + "/" + subcomponent},
        {"{component_include}", include},
        {"{component}", component},
        {"{header}", get_or(t, group, "header", "")},
        {"{entity}", singular},
        {"{EntityPascal}", snake_to_pascal(singular)}};
}

/**
 * @brief The placeholders of a model of the MASD library: its component's
 * directory and, for a table, its entity.
 */
std::vector<placeholder> masd_placeholders(const model_tables& t,
    value_id model) {
    return {{"{component_dir}", get_or(t, model, "product", "") + "." +
                get_or(t, model, "component", "")},
        {"{entity}", get_or(t, model, "entity_singular", "")}};
}

/**
 * @brief The placeholders of a masd.codegen.operation model, whose
 * component is under @p product.
 */
std::vector<placeholder> masd_operation_placeholders(const model_tables& t,
    value_id model, std::string_view product) {
    return {{"{component_dir}", under(product,
                get_or(t, model, "component", ""))},
        {"{entity}", get_or(t, model, "entity_singular", "")}};
}

/**
 * @brief The root of @p address: its first part.
 */
std::string_view root_of(std::string_view address) {
    return address.substr(0, address.find('.'));
}

/**
 * @brief resolve_targets' gates for the domain entity @p entity of the
 * model at @p model_path: drops from @p facets what the entity cannot
 * render, and returns whether its history archetypes are withheld, as
 * they are for a current-state entity.
 */
bool gate_entity(facet_set& facets, const types::physical_space& g,
    const types::checkout_registry& registry, const model_tables& t,
    value_id entity, std::string_view model_path) {
    if (values::truthy(t, entity, "sql_only")) {
        std::erase_if(facets, [&](const std::string& f) {
            const auto row = facet_row(g, f);
            return !row ||
                g.facets.technical_spaces[*row] != under(g.root, "sql");
        });
    }
    if (registry.protocol_owners.contains({std::string(parent(model_path)),
            std::string(values::str(t, entity, "component")),
            std::string(values::str(t, entity, "entity_singular"))}))
        drop(facets, g.root, protocol_facets);

    const auto sql = values::member(t, entity, "sql");
    const bool current_state = values::truthy(t, sql, "current_state");
    if (values::truthy(t, entity, "domain_audit_group") ||
        values::truthy(t, sql, "no_audit_columns") || current_state)
        drop(facets, g.root, no_flat_audit_facets);

    const auto profiles = values::str(t, entity, "profile");
    if (has_profile(profiles, "artefact-staging-only")) {
        const auto plural = values::str(t, entity, "entity_plural");
        const auto component = file_name(parent(parent(model_path)));
        const auto owners = registry.plural_owners.find(plural);
        if (!plural.empty() && owners != registry.plural_owners.end() &&
            std::ranges::any_of(owners->second,
                [&](const std::string& c) { return c != component; }))
            drop(facets, g.root, staging_only_facets);
        drop(facets, g.root, staging_only_eventing_facets);
    }
    if (has_profile(profiles, "read-model"))
        drop(facets, g.root, staging_only_eventing_facets);
    return current_state;
}

/**
 * @brief resolve_targets' gates for the junction @p junction of the
 * model at @p model_path: drops from @p facets what the junction cannot
 * render, and returns whether its notify trigger archetypes are withheld,
 * as they are for a junction that announces nothing.
 */
bool gate_junction(facet_set& facets, const types::physical_space& g,
    const types::checkout_registry& registry, const model_tables& t,
    value_id junction, std::string_view model_path) {
    if (values::truthy(t, junction, "sql_only")) {
        std::erase_if(facets, [&](const std::string& f) {
            const auto row = facet_row(g, f);
            return !row ||
                g.facets.technical_spaces[*row] != under(g.root, "sql");
        });
    }
    if (registry.protocol_owners.contains({std::string(parent(model_path)),
            std::string(values::str(t, junction, "component")),
            std::string(values::str(t, junction, "name_singular"))}))
        drop(facets, g.root, protocol_facets);
    if (!values::truthy(t, values::member(t, junction, "left"), "list_by") &&
        !values::truthy(t, values::member(t, junction, "right"), "list_by")) {
        drop(facets, g.root, junction_messaging_facets);
        drop(facets, g.root, protocol_facets);
    }
    if (!values::truthy(t, junction, "history"))
        drop(facets, g.root, junction_history_facets);
    const bool silent = !values::truthy(t, junction, "events");
    if (silent)
        drop(facets, g.root, junction_event_facets);
    return silent;
}

}

bool protocol_owned(const types::checkout_registry& registry,
    const model_tables& models, value_id entity, std::string_view model_path) {
    const auto singular = values::truthy(models, entity, "entity_singular") ?
        values::str(models, entity, "entity_singular") :
        values::str(models, entity, "name_singular");
    return registry.protocol_owners.contains({std::string(parent(model_path)),
        std::string(values::str(models, entity, "component")),
        std::string(singular)});
}

bool facet_enabled(std::span<const drawer_property> properties,
    std::string_view root, std::string_view facet,
    std::span<const std::string_view> addresses, bool fallback) {
    const auto o = enabled_overrides(properties, root);
    const auto ts = under(root, "cpp");
    return std::ranges::all_of(addresses, [&](std::string_view a) {
        return is_enabled(a, facet, ts, root, o, fallback);
    });
}

bool archetype_enabled(std::span<const drawer_property> properties,
    std::string_view root, std::string_view address, std::string_view facet,
    std::string_view ts) {
    const auto o = enabled_overrides(properties, root);
    const bool named = std::ranges::any_of(o, [&](const auto& entry) {
        const std::string_view key = entry.first;
        return entry.second && key.size() > facet.size() + 1 &&
            key.starts_with(facet) && key[facet.size()] == '.' &&
            key.find('.', facet.size() + 1) == std::string_view::npos;
    });
    return (named || is_enabled(facet, facet, ts, root, o, true)) &&
        is_enabled(address, facet, ts, root, o, true);
}

types::physical_space load_physical_space(std::string_view root,
    std::span<const node_document> nodes) {
    types::physical_space g;
    g.root = root;
    auto& f = g.facets;
    auto& a = g.archetypes;
    std::vector<std::optional<std::string>> raw_defaults;
    for (const auto& node : nodes) {
        const auto kw = keywords(node.text);
        auto stem = node.name;
        if (stem.ends_with(".org"))
            stem.remove_suffix(4);
        const auto address = get_or(kw, "title", stem);
        const auto type = get_or(kw, "type");
        if (type == "facet") {
            auto row = facet_row(g, address);
            if (!row) {
                row = f.size();
                f.addresses.push_back(address);
                f.technical_spaces.emplace_back();
                f.model_types.emplace_back();
                f.defaults.push_back(1);
            }
            f.technical_spaces[*row] = get_or(kw, "facet_group");
            f.model_types[*row] = words(get_or(kw, "model_types"));
            f.defaults[*row] = parse_default(get(kw, "default"), true);
        } else if (type == "archetype") {
            const auto facet = get_or(kw, "facet");
            auto model_types = words(get_or(kw, "model_types"));
            if (model_types.empty()) {
                if (const auto row = facet_row(g, facet))
                    model_types = f.model_types[*row];
            }
            a.addresses.push_back(address);
            a.facets.push_back(facet);
            a.template_names.push_back(tangled_template(node.text));
            a.outputs.push_back(get_or(kw, "output"));
            a.model_types.push_back(std::move(model_types));
            a.kinds.push_back(words(get_or(kw, "component_kind")));
            a.data_sources.push_back(get_or(kw, "data_source"));
            a.requires_facets.push_back(get_or(kw, "requires_facet"));
            raw_defaults.push_back(get(kw, "default"));
        }
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto row = facet_row(g, a.facets[i]);
        a.defaults.push_back(parse_default(raw_defaults[i],
            row ? f.defaults[*row] != 0 : true));
    }
    return g;
}

types::checkout_registry read_checkout_registry(std::string_view product,
    std::span<const checkout_file> files) {
    types::checkout_registry r;
    const auto shell = "projects/" + under(product, "shell") + "/";
    constexpr std::string_view overview = "/modeling/component_overview.org";
    for (const auto& f : files) {
        const auto dir = parent(f.path);
        if (org_type(f.text) == "ores.codegen.operation") {
            const auto kw = keywords(f.text);
            const auto component = get_or(kw, "component");
            const auto singular = get_or(kw, "entity_singular");
            if (!component.empty() && !singular.empty()) {
                for (const auto d : {dir, parent(dir)})
                    r.protocol_owners.emplace(std::string(d), component,
                        singular);
            }
        }
        const auto component = file_name(parent(dir));
        if (file_name(dir) == "modeling" && parent(parent(dir)) == "projects") {
            const auto plural = stated_plural(f.text);
            if (!plural.empty())
                r.plural_owners[std::string(plural)].emplace(component);
        }
        if (f.path.starts_with(shell) && f.path.ends_with(overview) &&
            f.path.size() == shell.size() + component.size() +
                overview.size()) {
            const auto kw = keywords(f.text);
            const auto v = lower(strip(get_or(kw, "shell_command_aggregator")));
            if (v == "true" || v == "t" || v == "yes" || v == "1")
                r.shell_registrars.emplace(component,
                    words(get_or(kw, "shell_command_local_units")));
        }
    }
    return r;
}

std::vector<unit> model_units(const types::physical_space& g,
    const types::checkout_registry& registry, std::string_view type,
    const model_tables& models, value_id model, std::string_view model_path,
    std::span<const drawer_property> properties) {
    auto o = enabled_overrides(properties, g.root);
    if (g.root != masd_root)
        o.merge(enabled_overrides(properties, masd_root));
    const auto& f = g.facets;
    const auto& a = g.archetypes;

    facet_set facets;
    for (std::size_t i = 0; i < f.size(); ++i) {
        if (!serves(f.model_types[i], type))
            continue;
        bool named = false;
        for (std::size_t j = 0; j < a.size(); ++j) {
            if (a.facets[j] != f.addresses[i])
                continue;
            const auto it = o.find(a.addresses[j]);
            named = named || (it != o.end() && it->second);
        }
        if (named || is_enabled(f.addresses[i], f.addresses[i],
                f.technical_spaces[i], root_of(f.addresses[i]), o,
                f.defaults[i] != 0))
            facets.insert(f.addresses[i]);
    }

    const auto owned = values::member(models, model, type);
    const bool is_entity = (type == "domain_entity" || type == "junction") &&
        !owned.is_none();
    std::span<const std::string_view> withheld;
    if (type == "domain_entity" &&
        gate_entity(facets, g, registry, models, owned, model_path))
        withheld = no_temporal_archetypes;
    if (type == "junction" &&
        gate_junction(facets, g, registry, models, owned, model_path))
        withheld = junction_event_archetypes;
    if (!facet_renders(g, under(g.root, "cpp.protocol"), type, o))
        drop(facets, g.root, protocol_dependent_facets);

    const bool is_component = type == "component" && !owned.is_none();
    const auto kind = is_component
        ? string_member(models, owned, "kind") : std::nullopt;
    const bool registrar = is_entity && registry.shell_registrars
        .contains(values::str(models, owned, "component"));
    const auto placeholders = owned.is_none() ? std::vector<placeholder>{}
        : type == "component"
        ? component_placeholders(models, owned, kind, model_path, g.root)
        : type == "domain_entity" ? entity_placeholders(models, owned, g.root)
        : type == "junction" ? junction_placeholders(models, owned, g.root)
        : type == "operation" ? operation_placeholders(models, owned, g.root)
        : type == "field_group"
        ? field_group_placeholders(models, owned, g.root)
        : type == "family" ? family_placeholders(models, owned, g.root)
        : type == "masd_entity" || type == "masd_schema" ||
            type == "masd_component"
        ? masd_placeholders(models, owned)
        : type == "masd_operation"
        ? masd_operation_placeholders(models, owned, g.root)
        : std::vector<placeholder>{};

    std::vector<std::size_t> deferred;
    std::vector<std::string> outputs;
    for (const auto& facet : facets) {
        const auto row = facet_row(g, facet);
        const std::string_view ts = row
            ? std::string_view(f.technical_spaces[*row]) : "";
        for (std::size_t j = 0; j < a.size(); ++j) {
            if (a.facets[j] != facet || !serves(a.model_types[j], type))
                continue;
            if (!a.kinds[j].empty() && (!kind ||
                std::ranges::find(a.kinds[j], *kind) == a.kinds[j].end()))
                continue;
            if (std::ranges::any_of(withheld, [&](std::string_view w) {
                    return a.addresses[j] == under(g.root, w);
                }))
                continue;
            if (!is_enabled(a.addresses[j], facet, ts, root_of(facet), o,
                    a.defaults[j] != 0))
                continue;
            if (a.template_names[j].empty() || a.outputs[j].empty())
                continue;
            if (!registrar && std::ranges::find(shell_aggregator_templates,
                    a.template_names[j]) != std::end(shell_aggregator_templates))
                continue;
            auto output = resolve(a.outputs[j], placeholders);
            if (!output)
                continue;
            deferred.push_back(j);
            outputs.push_back(std::move(*output));
        }
    }

    std::vector<unit> units;
    std::set<std::string, std::less<>> seen;
    for (std::size_t k = 0; k < deferred.size(); ++k) {
        const auto& requires_facet = a.requires_facets[deferred[k]];
        if (!requires_facet.empty() &&
            !facet_renders(g, requires_facet, type, o))
            continue;
        if (!seen.insert(outputs[k]).second)
            continue;
        units.push_back({a.template_names[deferred[k]], outputs[k]});
    }
    return units;
}

}

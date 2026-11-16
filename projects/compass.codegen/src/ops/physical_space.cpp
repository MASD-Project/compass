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
#include <map>
#include <optional>
#include <set>

namespace compass::codegen::ops {

namespace {

using compass::model::types::model_tables;
using compass::model::types::text_id;
using compass::model::types::value_id;
using compass::model::types::value_kind;

constexpr std::string_view protocol_dependent_facets[] = {
    "ores.cpp.http-route", "ores.doc.http-recipe", "ores.cpp.nats-eventing",
    "ores.cpp.nats-event-registrar", "ores.cpp.eventing-integration-test",
    "ores.cpp.shell-command", "ores.doc.shell-recipe"};

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

value_id member(const model_tables& t, value_id object, std::string_view key) {
    if (object.is_none() || t.values.kinds[object] != value_kind::object)
        return value_id::none();
    for (auto c = t.values.firsts[object]; !c.is_none();
         c = t.values.nexts[c]) {
        if (compass::model::types::text_of(t.texts, t.values.keys[c]) == key)
            return c;
    }
    return value_id::none();
}

std::optional<std::string_view> string_member(const model_tables& t,
    value_id object, std::string_view key) {
    const auto v = member(t, object, key);
    if (v.is_none() || t.values.kinds[v] != value_kind::string)
        return std::nullopt;
    return compass::model::types::text_of(t.texts,
        text_id{static_cast<std::uint32_t>(t.values.scalars[v])});
}

using overrides = std::map<std::string, bool, std::less<>>;

/**
 * @brief _enabled_overrides: each ores[.part]*.enabled key of the drawer.
 */
overrides enabled_overrides(std::span<const drawer_property> properties) {
    overrides out;
    constexpr std::string_view suffix = ".enabled";
    for (const auto& p : properties) {
        const auto key = strip(p.key);
        if (!key.ends_with(suffix))
            continue;
        const auto address = key.substr(0, key.size() - suffix.size());
        if (address != "ores" && !address.starts_with("ores."))
            continue;
        bool valid = true;
        for (std::size_t i = 4; i < address.size(); ++i) {
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
    std::string_view ts, const overrides& o, bool fallback) {
    for (const auto key : {address, facet, ts, std::string_view("ores")}) {
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
        row ? std::string_view(g.facets.technical_spaces[*row]) : "", o,
        row ? g.facets.defaults[*row] != 0 : true);
}

void replace_all(std::string& s, std::string_view from, std::string_view to) {
    for (auto p = s.find(from); p != std::string::npos;
         p = s.find(from, p + to.size()))
        s.replace(p, from.size(), to);
}

}

types::physical_space load_physical_space(
    std::span<const node_document> nodes) {
    types::physical_space g;
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

std::vector<unit> model_units(const types::physical_space& g,
    std::string_view type, const model_tables& models, value_id model,
    std::string_view model_path, std::span<const drawer_property> properties) {
    const auto o = enabled_overrides(properties);
    const auto& f = g.facets;
    const auto& a = g.archetypes;

    std::set<std::string, std::less<>> facets;
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
                f.technical_spaces[i], o, f.defaults[i] != 0))
            facets.insert(f.addresses[i]);
    }
    if (!facet_renders(g, "ores.cpp.protocol", type, o)) {
        for (const auto d : protocol_dependent_facets) {
            const auto it = facets.find(d);
            if (it != facets.end())
                facets.erase(it);
        }
    }

    const bool is_component = type == "component" &&
        !member(models, model, "component").is_none();
    const auto component = member(models, model, "component");
    const std::string name(string_member(models, component, "name")
        .value_or("unknown"));
    const auto declared_full = string_member(models, component, "full_name");
    const std::string full_name = declared_full
        ? std::string(*declared_full) : "ores." + name;
    const auto kind = is_component
        ? string_member(models, component, "kind") : std::nullopt;

    // resolve_targets sets the component's dir to its root relative to
    // projects/, the directory above the model's modeling/.
    std::string_view dir = model_path;
    for (int up = 0; up < 2; ++up)
        dir = dir.substr(0, dir.rfind('/') == std::string_view::npos
            ? 0 : dir.rfind('/'));
    if (dir.starts_with("projects/"))
        dir.remove_prefix(9);
    const std::string component_dir(dir);
    const std::string include_root = kind && *kind == "adapter" &&
        component_dir.find('/') != std::string::npos
        ? component_dir.substr(0, component_dir.find('/')) : full_name;

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
            if (!is_enabled(a.addresses[j], facet, ts, o, a.defaults[j] != 0))
                continue;
            if (a.template_names[j].empty() || a.outputs[j].empty())
                continue;
            if (std::ranges::find(shell_aggregator_templates,
                    a.template_names[j]) != std::end(shell_aggregator_templates))
                continue;
            auto output = a.outputs[j];
            if (is_component) {
                replace_all(output, "{component_dir}", component_dir);
                replace_all(output, "{component}", name);
                replace_all(output, "{component_full}", full_name);
                replace_all(output, "{component_include_root}",
                    include_root);
            }
            deferred.push_back(j);
            outputs.push_back(std::move(output));
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

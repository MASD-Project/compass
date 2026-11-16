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
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::value_id;

std::string doubled_quotes(std::string_view s) {
    std::string out;
    for (const char ch : s) {
        out += ch;
        if (ch == '\'')
            out += '\'';
    }
    return out;
}

/**
 * @brief The properties of @p n by lowered key, the last written winning.
 */
std::map<std::string, std::string_view> lowered(const outline& o, node_id n) {
    std::map<std::string, std::string_view> out;
    for (const auto& p : properties(o, n))
        out[to_lower(p.key)] = p.value;
    return out;
}

std::string_view get_or(const std::map<std::string, std::string_view>& props,
    const std::string& key, std::string_view fallback) {
    const auto it = props.find(key);
    return it == props.end() ? fallback : it->second;
}

std::int64_t integer(std::string_view raw) {
    const auto v = parse_int(raw);
    if (!v)
        throw model_error("not an integer: " + std::string(raw));
    return *v;
}

}

void load_asset_class_catalogue(load_context& c, value_id root) {
    auto& b = c.b;
    const auto& o = c.o;
    const node_id document{0};
    ensure_profile_binding(o);
    const auto catalogue = b.object(root, "asset_class_catalogue");
    for (const auto key : {"component", "brief"}) {
        if (const auto r = frontmatter(o, key); r.found)
            b.set(catalogue, key, r.value);
    }
    const auto taxonomy_section = section(o, document, "Taxonomy");
    if (taxonomy_section.is_none())
        throw model_error("no * Taxonomy section");
    const auto taxonomy = b.list(catalogue, "taxonomy");
    std::vector<std::string_view> codes;
    std::vector<std::int64_t> orders;
    std::vector<value_id> entries;
    for (const auto n : children(o, taxonomy_section)) {
        const auto props = lowered(o, n);
        const auto title = o.titles[n.value];
        const auto raw_order = props.find("display_order");
        const auto order = raw_order == props.end()
            ? static_cast<std::int64_t>(codes.size() + 1)
            : integer(raw_order->second);
        const auto entry = b.append_object(taxonomy);
        entries.push_back(entry);
        const std::string name(trim(get_or(props, "name", title)));
        const auto description = body(o, n);
        b.set(entry, "code", title);
        b.set(entry, "name", name);
        b.set_integer(entry, "display_order", order);
        b.set(entry, "description", description);
        b.set(entry, "description_sql",
            doubled_quotes(collapse_spaces(description)));
        b.set(entry, "badge_code", "asset_class_" + std::string(title));
        const auto badge_name = props.find("badge_name");
        b.set(entry, "badge_name", badge_name == props.end() ? name
            : std::string(trim(badge_name->second)));
        const auto badge_description = props.find("badge_description");
        b.set(entry, "badge_description", badge_description == props.end()
            ? name + " asset class."
            : std::string(trim(badge_description->second)));
        for (const auto key : {"badge_background", "badge_text",
            "badge_severity", "badge_css"})
            b.set(entry, key, trim(get_or(props, key, "")));
        b.set_integer(entry, "badge_order",
            integer(get_or(props, "badge_order", "0")));
        codes.push_back(title);
        orders.push_back(order);
    }
    if (std::set<std::string_view>(codes.begin(), codes.end()).size() !=
        codes.size())
        throw model_error("duplicate taxonomy code");
    if (std::set<std::int64_t>(orders.begin(), orders.end()).size() !=
        orders.size())
        throw model_error("duplicate taxonomy display_order");
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const bool last = i + 1 == entries.size();
        b.set(entries[i], "last", last);
        b.set(entries[i], "comma", last ? "" : ",");
    }

    const auto namespace_section = section(o, document, "Namespace");
    if (namespace_section.is_none())
        throw model_error("no * Namespace section");
    const auto ns = b.list(catalogue, "namespace");
    const auto to_class = b.object(catalogue, "authority_to_class");
    for (const auto n : children(o, namespace_section)) {
        const auto props = lowered(o, n);
        const auto code = trim(get_or(props, "refdata_code", ""));
        if (!code.empty() && std::ranges::find(codes, code) == codes.end())
            throw model_error("authority maps to an unknown taxonomy code");
        const auto entry = b.append_object(ns);
        b.set(entry, "authority", o.titles[n.value]);
        b.set(entry, "refdata_code", code);
        b.set(entry, "description", body(o, n));
        b.set(to_class, o.titles[n.value], code);
    }

    const auto domains = children(o, section(o, document, "Code domain"));
    if (domains.size() != 1)
        throw model_error("* Code domain must hold exactly one entry");
    const auto d = domains.front();
    const auto props = lowered(o, d);
    std::string codes_text;
    for (std::size_t i = 0; i < codes.size(); ++i) {
        if (i != 0)
            codes_text += ", ";
        codes_text += codes[i];
    }
    auto description = collapse_spaces(body(o, d));
    for (auto at = description.find("{codes}"); at != std::string::npos;
         at = description.find("{codes}", at + codes_text.size()))
        description.replace(at, 7, codes_text);
    const auto domain = b.object(catalogue, "code_domain");
    b.set(domain, "code", o.titles[d.value]);
    b.set(domain, "name", trim(get_or(props, "name", o.titles[d.value])));
    b.set_integer(domain, "display_order",
        integer(get_or(props, "display_order", "0")));
    b.set(domain, "description", description);
    b.set(domain, "description_sql", doubled_quotes(description));
}

}

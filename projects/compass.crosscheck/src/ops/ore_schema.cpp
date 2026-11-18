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
#include "compass.crosscheck/ops/ore_schema.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>

namespace compass::crosscheck::ops::ore_schema {

namespace {

namespace pt = compass::platform::text;

const std::string xs = "{http://www.w3.org/2001/XMLSchema}";

std::string_view attribute(const pt::xml_element& e, std::string_view name) {
    return pt::xml_document::get(e, name).value_or(std::string_view{});
}

/**
 * @brief The first child of @p e in the schema namespace with local name
 * @p local, as element.find(XS + local) gives it. The reader's own find
 * takes a path split at '/', which the namespace URI holds.
 */
const pt::xml_element* child(const pt::xml_document& doc,
    const pt::xml_element& e, std::string_view local) {
    for (const auto id : e.children)
        if (doc[id].tag.size() == xs.size() + local.size() &&
            doc[id].tag.starts_with(xs) && doc[id].tag.ends_with(local))
            return &doc[id];
    return nullptr;
}

bool is_container(const pt::xml_element& e) {
    return e.tag == xs + "sequence" || e.tag == xs + "choice" ||
        e.tag == xs + "all";
}

}

schemas::schemas(const std::vector<std::pair<std::string, std::string>>& files,
    std::map<std::string, std::string, std::less<>> kind_of)
    : kind_of_(std::move(kind_of)) {
    for (const auto& [name, text] : files) {
        auto xml = pt::xml_document::parse(text);
        if (!xml)
            continue;
        document d{std::move(*xml), {}, {}};
        const auto& root = d.xml.root();
        for (const auto id : root.children) {
            const auto& e = d.xml[id];
            const auto n = std::string(attribute(e, "name"));
            if (e.tag == xs + "complexType")
                d.complex_types.insert_or_assign(n, &e);
            else if (e.tag == xs + "element")
                d.tops.insert_or_assign(n, &e);
        }
        for (const auto id : root.children) {
            const auto& node = d.xml[id];
            if (node.tag != xs + "simpleType")
                continue;
            const auto type = pt::xml_document::get(node, "name");
            if (!type)
                continue;
            simple_type t;
            t.file = name;
            const auto at = text.find("name=\"" + std::string(*type) + "\"");
            t.line = at == std::string::npos ? 0
                : static_cast<std::size_t>(std::count(text.begin(),
                      text.begin() + static_cast<std::ptrdiff_t>(at), '\n')) + 1;
            for (const auto* item : d.xml.iter(node, xs + "enumeration"))
                if (const auto value = pt::xml_document::get(*item, "value"))
                    t.values.emplace_back(*value);
            if (const auto* r = child(d.xml, node, "restriction")) {
                if (const auto base = pt::xml_document::get(*r, "base"))
                    t.base = std::string(*base);
                const auto value_of = [&](std::string_view tag)
                    -> std::optional<std::string> {
                    const auto* f = child(d.xml, *r, tag);
                    if (f == nullptr)
                        return std::nullopt;
                    const auto value = pt::xml_document::get(*f, "value");
                    if (!value)
                        return std::nullopt;
                    return std::string(*value);
                };
                t.pattern = value_of("pattern");
                t.minimum = value_of("minInclusive");
                t.maximum = value_of("maxInclusive");
            }
            types_.try_emplace(std::string(*type), std::move(t));
        }
        documents_.emplace(name, std::move(d));
    }
}

void schemas::collect(const document& d, const pt::xml_element& holder,
    elements& found) const {
    for (const auto id : holder.children) {
        const auto& child = d.xml[id];
        if (child.tag == xs + "element") {
            const auto name = attribute(child, "name");
            if (!name.empty())
                found.try_emplace(std::string(name), &child);
        } else if (is_container(child)) {
            collect(d, child, found);
        }
    }
}

schemas::elements schemas::children(const document& d,
    const pt::xml_element& element) const {
    const pt::xml_element* holder = child(d.xml, element, "complexType");
    if (holder == nullptr) {
        const auto type = d.complex_types.find(attribute(element, "type"));
        if (type == d.complex_types.end())
            return {};
        holder = type->second;
    }
    elements found;
    for (const auto* tag : {"sequence", "choice", "all"})
        for (const auto id : holder->children)
            if (d.xml[id].tag == xs + tag)
                collect(d, d.xml[id], found);
    return found;
}

std::optional<std::string> schemas::declared(std::string_view name,
    std::string_view xpath) const {
    const auto doc = documents_.find(name);
    if (doc == documents_.end())
        return std::nullopt;
    const auto& d = doc->second;
    std::vector<std::string_view> parts;
    for (std::size_t at = 0; at <= xpath.size();) {
        const auto end = std::min(xpath.find('/', at), xpath.size());
        if (end > at)
            parts.push_back(xpath.substr(at, end - at));
        at = end + 1;
    }
    if (parts.empty())
        return std::nullopt;
    const auto top = d.tops.find(parts.front());
    if (top == d.tops.end())
        return std::nullopt;
    const pt::xml_element* element = top->second;
    for (std::size_t k = 1; k < parts.size(); ++k) {
        const auto found = children(d, *element);
        const auto next = found.find(parts[k]);
        if (next == found.end())
            return std::nullopt;
        element = next->second;
    }
    return std::string(attribute(*element, "type"));
}

std::optional<std::string> schemas::kind_of(std::string_view type) const {
    const auto k = kind_of_.find(type);
    if (k == kind_of_.end())
        return std::nullopt;
    return k->second;
}

std::optional<std::pair<std::string, std::string>> schemas::rule(
    std::string_view name, std::string_view xpath) const {
    const auto found = declared(name, xpath);
    if (!found)
        return std::nullopt;
    if (const auto kind = kind_of(*found))
        return std::pair{*kind, *found};
    if (const auto stated = restriction_of(*found))
        return std::pair{stated->kind, *found};
    return std::nullopt;
}

std::optional<restriction> schemas::restriction_of(std::string_view type) const {
    const auto found = types_.find(type);
    if (found == types_.end())
        return std::nullopt;
    const auto& t = found->second;
    if (!t.values.empty())
        return restriction{t, "enumerated"};
    const auto base = t.base ? kind_of(*t.base) : std::nullopt;
    if (t.pattern)
        return restriction{t, base.value_or("regex")};
    if (t.minimum || t.maximum)
        return restriction{t, base.value_or("range")};
    if (base)
        return restriction{t, *base};
    return std::nullopt;
}

}

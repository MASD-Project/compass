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
#ifndef COMPASS_CROSSCHECK_OPS_ORE_SCHEMA_HPP
#define COMPASS_CROSSCHECK_OPS_ORE_SCHEMA_HPP

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.platform/text/xml.hpp"

/**
 * @brief The types ORE's schemas declare for a leaf, as ORE Studio's
 * ore_schema_types.py reads them from external/ore/xsd: the walk from a
 * document's root element to a leaf through named and inline complex
 * types, and what each named simple type restricts.
 *
 * The walk collects the elements of a sequence, choice or all nested in
 * another at any depth, the first in document order winning. The script
 * looks one level too deep there and misses them; no leaf of ORE
 * 545cafca45 sits below such a nesting, so its seeds are the same.
 */
namespace compass::crosscheck::ops::ore_schema {

/**
 * @brief What one named simple type states: the file and line that
 * declare it, the values it enumerates, and the base, pattern and
 * bounds of its restriction.
 */
struct simple_type {
    std::string file;
    std::size_t line = 0;
    std::vector<std::string> values;
    std::optional<std::string> base;
    std::optional<std::string> pattern;
    std::optional<std::string> minimum;
    std::optional<std::string> maximum;
};

/**
 * @brief restriction(): the simple type and the kind of value it states.
 */
struct restriction {
    simple_type type;
    std::string kind;
};

class schemas final {
public:
    /**
     * @param files each schema's name and text, in byte order of the name.
     * @param kind_of the KIND_OF table: a type narrower than a string and
     * the kind of value it states.
     */
    schemas(const std::vector<std::pair<std::string, std::string>>& files,
        std::map<std::string, std::string, std::less<>> kind_of);

    /**
     * @brief declared(): the type attribute of the leaf at @p xpath of
     * schema @p name; nothing where the walk cannot reach it, and an empty
     * text where the leaf states no type.
     */
    [[nodiscard]] std::optional<std::string> declared(std::string_view name,
        std::string_view xpath) const;

    /**
     * @brief rule(): the kind of the leaf's value and the type that states
     * it, when narrower than a string.
     */
    [[nodiscard]] std::optional<std::pair<std::string, std::string>> rule(
        std::string_view name, std::string_view xpath) const;

    [[nodiscard]] std::optional<restriction> restriction_of(
        std::string_view type) const;

    /**
     * @brief KIND_OF.get(@p type).
     */
    [[nodiscard]] std::optional<std::string> kind_of(
        std::string_view type) const;

private:
    struct document {
        compass::platform::text::xml_document xml;
        std::map<std::string, const compass::platform::text::xml_element*,
            std::less<>> tops;
        std::map<std::string, const compass::platform::text::xml_element*,
            std::less<>> complex_types;
    };

    using elements = std::map<std::string,
        const compass::platform::text::xml_element*, std::less<>>;

    void collect(const document& d,
        const compass::platform::text::xml_element& holder,
        elements& found) const;
    [[nodiscard]] elements children(const document& d,
        const compass::platform::text::xml_element& element) const;

    std::map<std::string, document, std::less<>> documents_;
    std::map<std::string, simple_type, std::less<>> types_;
    std::map<std::string, std::string, std::less<>> kind_of_;
};

}

#endif

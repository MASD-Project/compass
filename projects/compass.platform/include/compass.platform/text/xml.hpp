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
#ifndef COMPASS_PLATFORM_TEXT_XML_HPP
#define COMPASS_PLATFORM_TEXT_XML_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * @brief XML documents read into the element tree Python's
 * xml.etree.ElementTree builds, through expat, the parser ElementTree
 * itself runs on, so text, tails, character references, CDATA and line
 * ends come out as the ported scripts see them.
 */
namespace compass::platform::text {

/**
 * @brief The index of an element in its document.
 */
using xml_element_id = std::uint32_t;

/**
 * @brief One element: its tag, attributes in document order, the text
 * before its first child, the text after its end tag up to the next tag
 * of its parent, and its children. A namespaced name is "{uri}local", as
 * ElementTree spells it. text and tail are nothing where ElementTree's
 * are None.
 */
struct xml_element {
    std::string tag;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::optional<std::string> text;
    std::optional<std::string> tail;
    std::vector<xml_element_id> children;
};

/**
 * @brief A parsed document; element 0 is the root. Comments and
 * processing instructions are dropped, as ElementTree's default builder
 * drops them.
 */
class xml_document final {
public:
    /**
     * @brief ET.fromstring(@p text), or nothing where it raises
     * ParseError.
     */
    [[nodiscard]] static std::optional<xml_document> parse(
        std::string_view text);

    [[nodiscard]] const xml_element& root() const noexcept {
        return elements_.front();
    }

    [[nodiscard]] const xml_element& operator[](xml_element_id id) const {
        return elements_[id];
    }

    /**
     * @brief element.get(@p name).
     */
    [[nodiscard]] static std::optional<std::string_view> get(
        const xml_element& e, std::string_view name) noexcept;

    /**
     * @brief element.findall(@p path) for a path of tag names separated
     * by '/': the matching descendants in document order.
     */
    [[nodiscard]] std::vector<const xml_element*> find_all(
        const xml_element& e, std::string_view path) const;

    /**
     * @brief element.find(@p path): the first of find_all, or null.
     */
    [[nodiscard]] const xml_element* find(const xml_element& e,
        std::string_view path) const;

    /**
     * @brief element.findtext(@p path): nothing when no element matches,
     * else its text, empty when it has none.
     */
    [[nodiscard]] std::optional<std::string> find_text(const xml_element& e,
        std::string_view path) const;

    /**
     * @brief element.iter(@p tag): the element and its descendants with
     * that tag, in document order.
     */
    [[nodiscard]] std::vector<const xml_element*> iter(const xml_element& e,
        std::string_view tag) const;

private:
    friend struct xml_builder;
    std::vector<xml_element> elements_;
};

}

#endif

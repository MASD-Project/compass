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
#include "compass.platform/text/xml.hpp"

#include <climits>
#include <memory>
#include <expat.h>

namespace compass::platform::text {

/**
 * @brief ElementTree's TreeBuilder: character data goes to the text of
 * the element last started, or to the tail of the element last ended.
 */
struct xml_builder {
    xml_document doc;
    std::vector<xml_element_id> open;
    std::string data;
    bool has_data = false;
    xml_element_id last = 0;
    bool tail = false;

    static std::string fix(const char* name) {
        std::string s(name);
        return s.find('}') == std::string::npos ? s : "{" + s;
    }

    void flush() {
        if (!has_data || doc.elements_.empty())
            return;
        auto& e = doc.elements_[last];
        (tail ? e.tail : e.text) = std::move(data);
        data.clear();
        has_data = false;
    }

    void start(const char* name, const char** attributes) {
        flush();
        const auto id = static_cast<xml_element_id>(doc.elements_.size());
        xml_element e;
        e.tag = fix(name);
        for (auto a = attributes; *a != nullptr; a += 2)
            e.attributes.emplace_back(fix(a[0]), a[1]);
        doc.elements_.push_back(std::move(e));
        if (!open.empty())
            doc.elements_[open.back()].children.push_back(id);
        open.push_back(id);
        last = id;
        tail = false;
    }

    void end() {
        flush();
        last = open.back();
        open.pop_back();
        tail = true;
    }

    void characters(const char* s, int n) {
        data.append(s, static_cast<std::size_t>(n));
        has_data = true;
    }
};

namespace {

void on_start(void* user, const XML_Char* name, const XML_Char** attributes) {
    static_cast<xml_builder*>(user)->start(name, attributes);
}

void on_end(void* user, const XML_Char*) {
    static_cast<xml_builder*>(user)->end();
}

void on_characters(void* user, const XML_Char* s, int n) {
    static_cast<xml_builder*>(user)->characters(s, n);
}

}

std::optional<xml_document> xml_document::parse(std::string_view text) {
    if (text.size() > static_cast<std::size_t>(INT_MAX))
        return std::nullopt;
    const std::unique_ptr<XML_ParserStruct, decltype(&XML_ParserFree)> parser(
        XML_ParserCreateNS("UTF-8", '}'), &XML_ParserFree);
    if (!parser)
        return std::nullopt;
    xml_builder b;
    XML_SetUserData(parser.get(), &b);
    XML_SetElementHandler(parser.get(), on_start, on_end);
    XML_SetCharacterDataHandler(parser.get(), on_characters);
    if (XML_Parse(parser.get(), text.data(), static_cast<int>(text.size()),
            XML_TRUE) != XML_STATUS_OK || b.doc.elements_.empty())
        return std::nullopt;
    return std::move(b.doc);
}

std::optional<std::string_view> xml_document::get(const xml_element& e,
    std::string_view name) noexcept {
    for (const auto& [key, value] : e.attributes)
        if (key == name)
            return value;
    return std::nullopt;
}

std::vector<const xml_element*> xml_document::find_all(const xml_element& e,
    std::string_view path) const {
    std::vector<const xml_element*> current{&e};
    std::size_t from = 0;
    while (true) {
        const auto slash = path.find('/', from);
        const auto step = path.substr(from, slash == std::string_view::npos
            ? std::string_view::npos : slash - from);
        std::vector<const xml_element*> next;
        for (const auto* parent : current)
            for (const auto child : parent->children)
                if (elements_[child].tag == step)
                    next.push_back(&elements_[child]);
        current = std::move(next);
        if (slash == std::string_view::npos)
            return current;
        from = slash + 1;
    }
}

const xml_element* xml_document::find(const xml_element& e,
    std::string_view path) const {
    const auto all = find_all(e, path);
    return all.empty() ? nullptr : all.front();
}

std::optional<std::string> xml_document::find_text(const xml_element& e,
    std::string_view path) const {
    const auto* found = find(e, path);
    if (found == nullptr)
        return std::nullopt;
    return found->text.value_or("");
}

std::vector<const xml_element*> xml_document::iter(const xml_element& e,
    std::string_view tag) const {
    std::vector<const xml_element*> out;
    std::vector<const xml_element*> stack{&e};
    while (!stack.empty()) {
        const auto* at = stack.back();
        stack.pop_back();
        if (at->tag == tag)
            out.push_back(at);
        for (auto it = at->children.rbegin(); it != at->children.rend(); ++it)
            stack.push_back(&elements_[*it]);
    }
    return out;
}

}

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
#include "compass.cpp/ops/declarations.hpp"

#include <vector>

namespace compass::cpp::ops {

namespace {

constexpr bool is_name_char(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_';
}

constexpr bool is_blank(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/**
 * @brief The tokens of one parameter without its name: the last token
 * goes when it is an identifier after a type, an identifier, &, * or >.
 */
std::string parameter_type(std::vector<std::string_view>& tokens) {
    if (tokens.size() >= 2 && is_name_char(tokens.back().front())) {
        const auto before = tokens[tokens.size() - 2];
        if (is_name_char(before.back()) || before == "&" || before == "*" ||
            before == ">")
            tokens.pop_back();
    }
    std::string out;
    for (const auto t : tokens)
        out += t;
    tokens.clear();
    return out;
}

/**
 * @brief The types of the parameters @p list declares, without their
 * names, default arguments or spaces, joined by commas: what tells two
 * overloads apart.
 */
std::string parameter_types(std::string_view list) {
    std::string out;
    std::vector<std::string_view> tokens;
    int depth = 0;
    bool defaulted = false;
    for (std::size_t i = 0; i < list.size();) {
        const char c = list[i];
        if (is_blank(c)) {
            ++i;
            continue;
        }
        if (c == ',' && depth == 0) {
            out += parameter_type(tokens) + ",";
            defaulted = false;
            ++i;
            continue;
        }
        if (c == '(' || c == '<' || c == '[' || c == '{')
            ++depth;
        else if (c == ')' || c == '>' || c == ']' || c == '}')
            --depth;
        else if (c == '=' && depth == 0)
            defaulted = true;
        auto e = i + 1;
        while (is_name_char(c) && e < list.size() && is_name_char(list[e]))
            ++e;
        if (!defaulted)
            tokens.push_back(list.substr(i, e - i));
        i = e;
    }
    return out + parameter_type(tokens);
}

}

std::map<std::string, std::uint32_t> defined_types(const source_model& m) {
    std::map<std::string, std::uint32_t> out;
    for (const auto& d : m.declarations) {
        if (d.kind != declaration_kind::type || !d.definition ||
            d.anonymous || d.qualified || d.scope.empty())
            continue;
        out.try_emplace(d.scope + "::" + std::string(d.name), d.offset);
    }
    return out;
}

std::map<std::pair<std::string, std::string>, std::uint32_t>
internal_names(const source_model& m) {
    std::map<std::pair<std::string, std::string>, std::uint32_t> out;
    for (const auto& d : m.declarations) {
        if (d.kind != declaration_kind::function &&
            d.kind != declaration_kind::variable)
            continue;
        if (d.is_extern || d.qualified)
            continue;
        const bool constant = (d.is_const || d.is_constexpr) && !d.is_inline;
        if (!d.anonymous && !d.is_static && !constant)
            continue;
        auto name = std::string(d.name);
        if (d.kind == declaration_kind::function)
            name += "(" + parameter_types(d.parameters) + ")";
        out.try_emplace({d.scope, std::move(name)}, d.start);
    }
    return out;
}

}

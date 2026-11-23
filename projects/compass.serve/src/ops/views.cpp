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
#include "compass.serve/ops/views.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace compass::serve::ops {

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.remove_suffix(1);
    return s;
}

std::string literal(std::string_view value) {
    std::string out = "'";
    for (const char c : value) {
        if (c == '\'')
            out += '\'';
        out += c;
    }
    return out + "'";
}

std::vector<std::string_view> words(std::string_view s) {
    std::vector<std::string_view> out;
    while (!(s = trim(s)).empty()) {
        const auto end = std::min(s.find_first_of(" \t"), s.size());
        out.push_back(s.substr(0, end));
        s.remove_prefix(end);
    }
    return out;
}

constexpr std::array<std::string_view, 4> single_fields{"type", "level", "audience", "state"};

}

std::vector<types::view_definition> default_views() {
    return {{"team", "all"},
            {"knowledge", "type: knowledge manual"},
            {"public", "audience: user"}};
}

std::expected<std::string, std::string> compile_view(std::string_view documents) {
    documents = trim(documents);
    if (documents == "all")
        return std::string("true");
    if (documents.empty())
        return std::unexpected(std::string("it states no documents; write all, or "
                                           "conditions such as type: knowledge manual"));
    std::string out;
    while (!documents.empty()) {
        const auto end = std::min(documents.find(';'), documents.size());
        const auto condition = trim(documents.substr(0, end));
        documents.remove_prefix(std::min(end + 1, documents.size()));
        const auto colon = condition.find(':');
        if (colon == std::string_view::npos)
            return std::unexpected("'" + std::string(condition) +
                                   "' is not a field, a colon and its values");
        const auto field = trim(condition.substr(0, colon));
        const auto values = words(condition.substr(colon + 1));
        if (values.empty())
            return std::unexpected("the field " + std::string(field) + " lists no values");
        std::string list;
        for (const auto v : values)
            list += (list.empty() ? "" : ", ") + literal(v);
        std::string sql;
        if (std::ranges::find(single_fields, field) != single_fields.end())
            sql = "d." + std::string(field) + " IN (" + list + ")";
        else if (field == "tag")
            sql = "d.tags && ARRAY[" + list + "]::text[]";
        else
            return std::unexpected("the field " + std::string(field) +
                                   " is not one of type, level, audience, state and tag");
        out += (out.empty() ? "" : " AND ") + sql;
    }
    return out;
}

std::expected<std::map<std::string, db::types::view_filter, std::less<>>, std::string>
compile_views(std::span<const types::view_definition> views) {
    std::map<std::string, db::types::view_filter, std::less<>> out;
    for (const auto& v : views) {
        auto condition = compile_view(v.documents);
        if (!condition)
            return std::unexpected("the view " + v.name + ": " + condition.error());
        if (!out.emplace(v.name, db::types::view_filter{std::move(*condition)}).second)
            return std::unexpected("the view " + v.name + " is declared twice");
    }
    return out;
}

}

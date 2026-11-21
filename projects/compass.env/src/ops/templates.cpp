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
#include "compass.env/ops/templates.hpp"

#include <format>

namespace compass::env::ops {

namespace {

bool name_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '.';
}

/**
 * @brief A {name} or {name:-default} at the start of @p text, which opens
 * with '{'.
 */
struct placeholder {
    std::string_view name;
    std::optional<std::string_view> fallback;
    std::size_t size = 0;
};

std::expected<placeholder, std::string> placeholder_at(std::string_view text) {
    std::size_t i = 1;
    while (i < text.size() && name_char(text[i]))
        ++i;
    placeholder p{.name = text.substr(1, i - 1)};
    if (p.name.empty())
        return std::unexpected(std::format("'{{' at \"{}\" opens no name; "
            "write {{{{ for a brace", text.substr(0, 20)));
    if (text.substr(i).starts_with(":-")) {
        const auto close = text.find('}', i + 2);
        if (close == std::string_view::npos)
            return std::unexpected(std::format("\"{}\" is not closed",
                text.substr(0, 20)));
        p.fallback = text.substr(i + 2, close - i - 2);
        p.size = close + 1;
        return p;
    }
    if (i >= text.size() || text[i] != '}')
        return std::unexpected(std::format("\"{}\" is not a name in braces",
            text.substr(0, 20)));
    p.size = i + 1;
    return p;
}

}

std::expected<std::string, std::string> expand(std::string_view text,
    const resolver& resolve) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        const auto rest = text.substr(i);
        if (rest.starts_with("{{") || rest.starts_with("}}")) {
            out.push_back(rest[0]);
            i += 2;
            continue;
        }
        if (rest[0] != '{') {
            out.push_back(rest[0]);
            ++i;
            continue;
        }
        const auto p = placeholder_at(rest);
        if (!p)
            return std::unexpected(p.error());
        const auto value = resolve(p->name);
        if (!value)
            return std::unexpected(std::format("no input or variable is "
                "named {}", p->name));
        out += value->empty() && p->fallback ? std::string(*p->fallback)
                                             : *value;
        i += p->size;
    }
    return out;
}

std::expected<std::vector<std::string>, std::string> names_in(
    std::string_view text) {
    std::vector<std::string> out;
    const auto collected = expand(text, [&](std::string_view name) {
        out.emplace_back(name);
        return std::optional<std::string>(std::string{});
    });
    if (!collected)
        return std::unexpected(collected.error());
    return out;
}

}

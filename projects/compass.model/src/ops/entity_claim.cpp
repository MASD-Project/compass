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
#include "compass.model/ops/entity_claim.hpp"

#include "text.hpp"

namespace compass::model::ops {

namespace {

bool is_lower_or_digit(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

/*
 * ^#\+<key>:\s*(\S+), multi-line: the whitespace may run over line ends.
 */
std::string first_keyword_token(std::string_view text, std::string_view key) {
    const auto marker = "#+" + std::string(key) + ":";
    for (auto at = text.find(marker); at != std::string_view::npos;
         at = text.find(marker, at + 1)) {
        if (at != 0 && text[at - 1] != '\n')
            continue;
        auto p = at + marker.size();
        while (p < text.size() && is_space(text[p]))
            ++p;
        auto end = p;
        while (end < text.size() && !is_space(text[end]))
            ++end;
        if (end > p)
            return std::string(text.substr(p, end - p));
    }
    return {};
}

/*
 * :tablename:\s+([a-z][a-z0-9_]*_tbl), first match: the longest run of
 * name characters that ends in _tbl.
 */
std::string first_tablename(std::string_view text) {
    constexpr std::string_view marker = ":tablename:";
    constexpr std::string_view suffix = "_tbl";
    for (auto at = text.find(marker); at != std::string_view::npos;
         at = text.find(marker, at + 1)) {
        auto p = at + marker.size();
        const auto spaced = p;
        while (p < text.size() && is_space(text[p]))
            ++p;
        if (p == spaced || p == text.size() || text[p] < 'a' || text[p] > 'z')
            continue;
        auto end = p + 1;
        while (end < text.size() && is_lower_or_digit(text[end]))
            ++end;
        for (auto stop = end; stop >= p + 1 + suffix.size(); --stop) {
            if (text.substr(stop - suffix.size(), suffix.size()) == suffix)
                return std::string(text.substr(p, stop - p));
        }
    }
    return {};
}

}

std::optional<entity_claim> entity_claim_of(std::string_view org_text) {
    auto singular = first_keyword_token(org_text, "entity_singular");
    if (singular.empty())
        return std::nullopt;
    auto table = first_tablename(org_text);
    if (table.empty())
        return std::nullopt;
    auto plural = first_keyword_token(org_text, "entity_plural");
    if (plural.empty())
        plural = singular + "s";
    return entity_claim{std::move(table), std::move(singular),
        std::move(plural)};
}

}

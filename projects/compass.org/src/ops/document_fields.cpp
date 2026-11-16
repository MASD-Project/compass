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
#include "compass.org/ops/document_fields.hpp"

#include <cstdint>

namespace compass::org::ops {

namespace {

bool is_blank(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

}

std::string_view name_text(const types::tables_view& t,
    types::name_id n) noexcept {
    const auto first = t.names.firsts[n];
    const auto end = n.next().value < t.names.size()
        ? t.names.firsts[n.next()]
        : static_cast<std::uint32_t>(t.name_bytes.size());
    return {t.name_bytes.data() + first, end - first};
}

types::keyword_id find_keyword(const types::tables_view& t,
    std::string_view key) noexcept {
    for (std::uint32_t k = 0; k < t.keywords.size(); ++k) {
        const types::keyword_id id{k};
        if (name_text(t, t.keywords.names[id]) == key)
            return id;
    }
    return types::keyword_id::none();
}

types::property_id find_document_property(const types::tables_view& t,
    std::string_view key) noexcept {
    const auto end = t.headings.size() == 0
        ? static_cast<std::uint32_t>(t.properties.size())
        : t.headings.property_firsts[types::heading_id{0}].value;
    for (std::uint32_t p = 0; p < end; ++p) {
        const types::property_id id{p};
        if (name_text(t, t.properties.names[id]) == key)
            return id;
    }
    return types::property_id::none();
}

types::source_range trimmed(std::string_view source,
    types::source_range range) noexcept {
    while (!range.empty() && is_blank(source[range.begin]))
        ++range.begin;
    while (!range.empty() && is_blank(source[range.end - 1]))
        --range.end;
    return range;
}

}

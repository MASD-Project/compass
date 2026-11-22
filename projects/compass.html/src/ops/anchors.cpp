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
#include "compass.html/ops/anchors.hpp"

#include <cstdint>
#include <unordered_set>
#include "objects.hpp"

namespace compass::html::ops {

using namespace org::types;

namespace {

bool is_slug_byte(unsigned char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 0x80;
}

void append_slug(std::string& out, std::string_view text) {
    bool hyphen = !out.empty();
    for (const char ch : text) {
        auto c = static_cast<unsigned char>(ch);
        if (c >= 'A' && c <= 'Z')
            c = static_cast<unsigned char>(c - 'A' + 'a');
        if (!is_slug_byte(c)) {
            hyphen = !out.empty();
            continue;
        }
        if (hyphen)
            out += '-';
        hyphen = false;
        out += static_cast<char>(c);
    }
}

}

std::vector<std::string> heading_anchors(std::string_view source,
    const tables_view& t) {
    const auto count = static_cast<std::uint32_t>(t.headings.size());
    std::vector<std::uint32_t> heading_of(t.elements.size(),
        heading_id::none_value);
    for (std::uint32_t h = 0; h < count; ++h)
        heading_of[t.headings.elements[heading_id{h}].value] = h;

    std::vector<std::string> slugs(count);
    std::vector<std::string> anchors(count);
    std::unordered_set<std::string> taken;
    for (std::uint32_t i = 0; i < count; ++i) {
        const heading_id h{i};
        const auto e = t.headings.elements[h];
        const auto parent = t.elements.parents[e];
        if (!parent.is_none() &&
            heading_of[parent.value] != heading_id::none_value)
            slugs[i] = slugs[heading_of[parent.value]];
        std::string title;
        detail::append_plain_text(title, source, t, t.headings.titles[h],
            detail::own_objects(t, e));
        append_slug(slugs[i], title);

        const auto id = detail::heading_property(source, t, h, "ID");
        if (!id.empty() && taken.insert(std::string(id)).second) {
            anchors[i] = id;
            continue;
        }
        const std::string base = slugs[i].empty() ? "section" : slugs[i];
        auto anchor = base;
        for (std::uint32_t n = 2; !taken.insert(anchor).second; ++n)
            anchor = base + "-" + std::to_string(n);
        anchors[i] = std::move(anchor);
    }
    return anchors;
}

}

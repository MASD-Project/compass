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
#include "compass.docs/ops/org_text.hpp"

#include <algorithm>
#include "compass.cpp/ops/text.hpp"
#include "compass.org/ops/document_fields.hpp"

namespace compass::docs::ops {

namespace {

namespace ct = compass::codec::types;
namespace ot = compass::org::types;
namespace oo = compass::org::ops;

}

std::string_view keyword_value(const ct::document& d, std::string_view key) {
    const auto k = oo::find_keyword(d.tables, key);
    if (k.is_none())
        return {};
    return oo::trimmed(d.source, d.tables.keywords.values[k]).of(d.source);
}

std::string_view first_word(std::string_view s) noexcept {
    const auto end = std::ranges::find_if(s, compass::cpp::ops::is_space);
    return s.substr(0, static_cast<std::size_t>(end - s.begin()));
}

bool has_tag(std::string_view filetags, std::string_view tag) noexcept {
    std::size_t start = 0;
    while (start <= filetags.size()) {
        auto end = filetags.find(':', start);
        if (end == std::string_view::npos)
            end = filetags.size();
        if (compass::cpp::ops::strip(filetags.substr(start, end - start)) ==
            tag)
            return true;
        start = end + 1;
    }
    return false;
}

std::uint32_t line_start(std::string_view source,
    std::uint32_t offset) noexcept {
    if (offset == 0)
        return 0;
    const auto nl = source.rfind('\n', offset - 1);
    return nl == std::string_view::npos ? 0
        : static_cast<std::uint32_t>(nl + 1);
}

std::vector<comment> comments(const ct::document& d) {
    const auto& e = d.tables.elements;
    std::vector<comment> out;
    for (std::uint32_t i = 0; i < e.size(); ++i) {
        const ot::element_id id{i};
        if (e.kinds[id] != ot::element_kind::comment)
            continue;
        const auto range = e.ranges[id];
        auto& c = out.emplace_back();
        c.begin = range.begin;
        for (const auto line : compass::cpp::ops::split_lines(
                 range.of(d.source))) {
            if (compass::cpp::ops::strip(line).starts_with('#'))
                c.lines.push_back(line);
        }
    }
    return out;
}

std::vector<bracket_link> bracket_links(const ct::document& d,
    std::string_view type) {
    const auto& t = d.tables;
    const auto& links = t.links;
    std::vector<bracket_link> out;
    for (std::uint32_t i = 0; i < links.size(); ++i) {
        const ot::link_id l{i};
        if (links.formats[l] != ot::link_format::bracket ||
            (!type.empty() && oo::name_text(t, links.types[l]) != type))
            continue;
        const auto object = links.objects[l];
        out.push_back({t.objects.ranges[object].begin,
            oo::trimmed(d.source, links.paths[l]).of(d.source),
            t.objects.contents[object].of(d.source)});
    }
    std::ranges::stable_sort(out, {}, &bracket_link::begin);
    return out;
}

}

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
#include "compass.cli/config/org_reader.hpp"

#include <algorithm>
#include "compass.org/ops/document_fields.hpp"

namespace compass::cli::config {

namespace ot = compass::org::types;
using compass::org::ops::trimmed;

std::string_view verbatim(std::string_view cell) noexcept {
    for (const char mark : {'=', '~'})
        if (cell.size() >= 2 && cell.front() == mark && cell.back() == mark)
            return cell.substr(1, cell.size() - 2);
    return cell;
}

std::string_view trim(std::string_view s) noexcept {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") + 1 - first);
}

std::uint32_t line_of(std::string_view text, std::uint32_t offset) noexcept {
    return static_cast<std::uint32_t>(
        1 + std::ranges::count(text.substr(0, offset), '\n'));
}

ot::heading_id org_reader::heading(std::string_view title) const {
    for (std::uint32_t h = 0; h < t.headings.size(); ++h) {
        const ot::heading_id id{h};
        if (t.headings.levels[id] == 1 &&
            trimmed(text, t.headings.titles[id]).of(text) == title)
            return id;
    }
    return ot::heading_id::none();
}

ot::element_id org_reader::table_under(std::string_view title) const {
    for (std::uint32_t i = 0; i < t.headings.size(); ++i) {
        const ot::heading_id id{i};
        if (t.headings.levels[id] != 1 ||
            trimmed(text, t.headings.titles[id]).of(text) != title)
            continue;
        const auto h = t.headings.elements[id];
        for (auto e = h.next(); e.value < t.elements.subtree_ends[h].value;
             e = e.next()) {
            if (t.elements.kinds[e] == ot::element_kind::table)
                return e;
        }
    }
    return ot::element_id::none();
}

std::vector<std::string_view> org_reader::cells(ot::element_id row) const {
    const auto first = t.elements.object_firsts[row].value;
    const auto end = row.next().value < t.elements.size()
        ? t.elements.object_firsts[row.next()].value
        : static_cast<std::uint32_t>(t.objects.size());
    std::vector<std::string_view> r;
    for (auto o = first; o < end;) {
        const ot::object_id id{o};
        if (t.objects.kinds[id] != ot::object_kind::table_cell) {
            ++o;
            continue;
        }
        auto cell = trim(t.objects.ranges[id].of(text));
        if (cell.ends_with('|'))
            cell.remove_suffix(1);
        r.push_back(trim(cell));
        o = t.objects.subtree_ends[id].value;
    }
    return r;
}

std::vector<ot::element_id> org_reader::rows(ot::element_id table,
    std::string_view header) const {
    std::vector<ot::element_id> out;
    bool first = true;
    for (auto row = table.next();
         row.value < t.elements.subtree_ends[table].value;
         row = t.elements.subtree_ends[row]) {
        if (t.elements.kinds[row] != ot::element_kind::table_row)
            continue;
        const auto c = cells(row);
        const auto is_header = first && !c.empty() && c[0] == header;
        first = false;
        if (!is_header)
            out.push_back(row);
    }
    return out;
}

}

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
#include "objects.hpp"

#include <algorithm>
#include "compass.org/ops/document_fields.hpp"

namespace compass::html::ops::detail {

using namespace org::types;

namespace {

bool is_blank(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

}

object_run own_objects(const tables_view& t, element_id e) noexcept {
    const auto next = e.next();
    const object_id last = next.value < t.elements.size()
        ? t.elements.object_firsts[next]
        : object_id{static_cast<std::uint32_t>(t.objects.size())};
    return {t.elements.object_firsts[e], last};
}

object_run nested_objects(const tables_view& t, object_id o) noexcept {
    return {o.next(), t.objects.subtree_ends[o]};
}

source_range without_post_blanks(std::string_view source,
    const tables_view& t, object_id o) noexcept {
    auto r = t.objects.ranges[o];
    const auto blanks = t.objects.post_blanks[o];
    if (blanks != post_blank_saturated) {
        r.end -= blanks;
        return r;
    }
    while (r.end > r.begin && is_blank(source[r.end - 1]))
        --r.end;
    return r;
}

link_id link_of(const tables_view& t, object_id o) noexcept {
    const auto& objects = t.links.objects.values;
    const auto it = std::lower_bound(objects.begin(), objects.end(), o);
    if (it == objects.end() || *it != o)
        return link_id::none();
    return link_id{static_cast<std::uint32_t>(it - objects.begin())};
}

std::string link_path(std::string_view source, const tables_view& t,
    link_id l) {
    return unescaped(t.links.paths[l].of(source));
}

std::string link_text(std::string_view source, const tables_view& t,
    object_id o) {
    const auto r = without_post_blanks(source, t, o);
    const auto l = link_of(t, o);
    const auto format = l.is_none() ? link_format::plain : t.links.formats[l];
    switch (format) {
    case link_format::plain: return unescaped(r.of(source));
    case link_format::angle:
        return unescaped(source.substr(r.begin + 1, r.size() - 2));
    case link_format::bracket:
        return unescaped(source.substr(r.begin + 2, r.size() - 4));
    }
    return {};
}

std::string unescaped(std::string_view raw) {
    std::string r;
    r.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const char c = raw[i];
        if (c == '\\' && i + 1 < raw.size() &&
            (raw[i + 1] == '[' || raw[i + 1] == ']')) {
            r += raw[++i];
        } else if (c == '\n' || c == '\r') {
            while (i + 1 < raw.size() && is_blank(raw[i + 1]))
                ++i;
            r += ' ';
        } else {
            r += c;
        }
    }
    return r;
}

void append_plain_text(std::string& out, std::string_view source,
    const tables_view& t, source_range contents, object_run run) {
    auto cursor = contents.begin;
    for (auto o = run.first; o < run.last; o = t.objects.subtree_ends[o]) {
        const auto r = without_post_blanks(source, t, o);
        out += source.substr(cursor, r.begin - cursor);
        cursor = r.end;
        switch (t.objects.kinds[o]) {
        case object_kind::bold:
        case object_kind::italic:
        case object_kind::underline:
        case object_kind::strike_through:
        case object_kind::subscript:
        case object_kind::superscript:
            if (!t.objects.contents[o].empty())
                append_plain_text(out, source, t, t.objects.contents[o],
                    nested_objects(t, o));
            break;
        case object_kind::code:
        case object_kind::verbatim:
            if (r.size() >= 2)
                out += source.substr(r.begin + 1, r.size() - 2);
            break;
        case object_kind::link: {
            const auto c = t.objects.contents[o];
            if (!c.empty())
                append_plain_text(out, source, t, c, nested_objects(t, o));
            else
                out += link_text(source, t, o);
            break;
        }
        default:
            out += r.of(source);
            break;
        }
    }
    if (cursor < contents.end)
        out += source.substr(cursor, contents.end - cursor);
}

std::string_view heading_property(std::string_view source,
    const tables_view& t, heading_id h, std::string_view key) {
    const auto first = t.headings.property_firsts[h].value;
    const auto next = h.next();
    const auto last = next.value < t.headings.size()
        ? t.headings.property_firsts[next].value
        : static_cast<std::uint32_t>(t.properties.size());
    for (auto p = first; p < last; ++p) {
        const property_id id{p};
        if (org::ops::name_text(t, t.properties.names[id]) == key)
            return org::ops::trimmed(source, t.properties.values[id])
                .of(source);
    }
    return {};
}

}

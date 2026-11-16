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
#include "compass.model/io/json.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

namespace compass::model::io {

namespace {

using types::value_id;
using types::value_kind;

void write_string(std::string& out, std::string_view s) {
    constexpr char hex[] = "0123456789abcdef";
    out += '"';
    for (const char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                out += "\\u00";
                out += hex[(c >> 4) & 0xF];
                out += hex[c & 0xF];
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

void indent(std::string& out, int depth) {
    out += '\n';
    out.append(static_cast<std::size_t>(depth) * 2, ' ');
}

void write_value(std::string& out, const types::model_tables& t, value_id v,
    int depth) {
    const auto& values = t.values;
    switch (values.kinds[v]) {
    case value_kind::null: out += "null"; return;
    case value_kind::boolean:
        out += values.scalars[v] != 0 ? "true" : "false";
        return;
    case value_kind::integer:
        out += std::to_string(values.scalars[v]);
        return;
    case value_kind::string:
        write_string(out, types::text_of(t.texts,
            types::text_id{static_cast<std::uint32_t>(values.scalars[v])}));
        return;
    case value_kind::list:
    case value_kind::object:
        break;
    }
    std::vector<value_id> children;
    for (auto c = values.firsts[v]; !c.is_none(); c = values.nexts[c])
        children.push_back(c);
    const bool object = values.kinds[v] == value_kind::object;
    if (children.empty()) {
        out += object ? "{}" : "[]";
        return;
    }
    if (object) {
        std::ranges::sort(children, [&](value_id a, value_id b) {
            return types::text_of(t.texts, values.keys[a]) <
                types::text_of(t.texts, values.keys[b]);
        });
    }
    out += object ? '{' : '[';
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (i != 0)
            out += ',';
        indent(out, depth + 1);
        if (object) {
            write_string(out, types::text_of(t.texts,
                values.keys[children[i]]));
            out += ": ";
        }
        write_value(out, t, children[i], depth + 1);
    }
    indent(out, depth);
    out += object ? '}' : ']';
}

}

void write_json(std::string& out, const types::model_tables& t,
    value_id value) {
    write_value(out, t, value, 0);
    out += '\n';
}

}

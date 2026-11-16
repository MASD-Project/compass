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
#include <algorithm>
#include <string>
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

using types::value_id;
using types::value_kind;

std::optional<std::int64_t> parse_int(std::string_view s) noexcept {
    s = trim(s);
    bool negative = false;
    if (!s.empty() && (s.front() == '+' || s.front() == '-')) {
        negative = s.front() == '-';
        s.remove_prefix(1);
    }
    if (s.empty() || s.front() == '_' || s.back() == '_')
        return std::nullopt;
    std::int64_t v = 0;
    char previous = 0;
    for (const char c : s) {
        if (c == '_') {
            if (previous == '_')
                return std::nullopt;
        } else if (c >= '0' && c <= '9') {
            v = v * 10 + (c - '0');
        } else {
            return std::nullopt;
        }
        previous = c;
    }
    return negative ? -v : v;
}

value_kind typed_kind(std::string_view raw) noexcept {
    if (iequals(raw, "true") || iequals(raw, "false"))
        return value_kind::boolean;
    if (parse_int(raw))
        return value_kind::integer;
    return value_kind::string;
}

void set_typed(builder& b, value_id parent, std::string_view key,
    std::string_view raw) {
    switch (typed_kind(raw)) {
    case value_kind::boolean:
        b.set(parent, key, iequals(raw, "true"));
        return;
    case value_kind::integer:
        b.set_integer(parent, key, *parse_int(raw));
        return;
    default:
        b.set(parent, key, raw);
    }
}

std::vector<std::string_view> lines_of(std::string_view text) {
    std::vector<std::string_view> lines;
    std::vector<std::uint32_t> starts;
    split_lines(text, lines, starts);
    return lines;
}

void includes_from_named_block(load_context& c, node_id n, value_id list) {
    const auto code = named_block(c.o, n, "includes");
    for (const auto raw : lines_of(code.value)) {
        const auto line = trim(raw);
        if (line.starts_with("#include"))
            c.b.append(list, trim(line.substr(8)));
    }
}

void ensure_profile_binding(const outline& o) {
    if (file_property(o, "profile").found)
        throw model_error(":profile: found in file-level :PROPERTIES: drawer");
    for (const auto key : o.frontmatter_keys) {
        if (iequals(key, "profile"))
            throw model_error("#+profile: found in the file frontmatter");
    }
}

void reject_junction_only_flags(const outline& o, std::string_view kind,
    bool root_allowed) {
    std::vector<node_id> drawers;
    const node_id root{0};
    if (!root_allowed)
        drawers.push_back(section(o, root, "Flags"));
    const auto cpp = section(o, root, "C++");
    if (!cpp.is_none())
        drawers.push_back(section(o, cpp, "Flags"));
    for (const auto n : drawers) {
        for (const auto& p : properties(o, n)) {
            if (iequals(p.key, "client_read_only"))
                throw model_error(":client_read_only: found in a " +
                    std::string(kind) + " model");
        }
    }
}

std::string_view member_name(const builder& b, value_id member) {
    const auto& t = b.tables();
    for (const auto key : {"name", "column"}) {
        const auto v = b.find(member, key);
        if (!v.is_none() && t.values.kinds[v] == value_kind::string) {
            const auto text = types::text_of(t.texts, types::text_id{
                static_cast<std::uint32_t>(t.values.scalars[v])});
            if (!text.empty())
                return text;
        }
    }
    return {};
}

economic economic_declaration(const builder& b, std::string_view owner,
    const std::vector<value_id>& members,
    const std::vector<value_id>& carried) {
    for (const auto c : carried) {
        if (!b.find(c, "economic").is_none())
            throw model_error(std::string(owner) +
                ": a carried column declares ':economic:'");
    }
    economic out;
    std::size_t declared = 0;
    for (const auto m : members) {
        const auto v = b.find(m, "economic");
        if (v.is_none())
            continue;
        ++declared;
        if (b.tables().values.kinds[v] != value_kind::boolean)
            throw model_error(std::string(owner) +
                ": ':economic:' is not true or false");
    }
    if (declared == 0)
        return out;
    if (declared != members.size())
        throw model_error(std::string(owner) +
            ": declares ':economic:' on some columns but not all");
    out.declared = true;
    for (const auto m : members) {
        if (b.tables().values.scalars[b.find(m, "economic")] != 0)
            out.members.emplace_back(member_name(b, m));
    }
    return out;
}

void write_economic(builder& b, value_id parent, const economic& e,
    bool canonical) {
    b.set(parent, "has_economic_declaration", e.declared);
    const auto fields = b.list(parent, "economic_fields");
    for (const auto& m : e.members)
        b.append(fields, m);
    b.set(parent, "has_economic_fields", !e.members.empty());
    if (!canonical)
        return;
    const auto members = b.list(parent, "canonical_members");
    for (const auto& m : e.members)
        b.append(members, m);
    b.set(parent, "has_canonical_members", !e.members.empty());
}

std::string text_value(const builder& b, value_id v) {
    const auto& t = b.tables();
    switch (t.values.kinds[v]) {
    case value_kind::string:
        return std::string(types::text_of(t.texts, types::text_id{
            static_cast<std::uint32_t>(t.values.scalars[v])}));
    case value_kind::boolean:
        return t.values.scalars[v] != 0 ? "True" : "False";
    case value_kind::integer:
        return std::to_string(t.values.scalars[v]);
    case value_kind::null:
        return "None";
    default:
        return {};
    }
}

bool truthy(const builder& b, value_id v) {
    if (v.is_none())
        return false;
    const auto& t = b.tables().values;
    switch (t.kinds[v]) {
    case value_kind::null: return false;
    case value_kind::boolean:
    case value_kind::integer:
        return t.scalars[v] != 0;
    case value_kind::string:
        return !string_of(b, v).empty();
    case value_kind::list:
    case value_kind::object:
        return !t.firsts[v].is_none();
    }
    return false;
}

std::string_view string_of(const builder& b, value_id v) {
    if (v.is_none())
        return {};
    const auto& t = b.tables();
    if (t.values.kinds[v] != value_kind::string)
        return {};
    return types::text_of(t.texts,
        types::text_id{static_cast<std::uint32_t>(t.values.scalars[v])});
}

lookup org_row::get(std::string_view key) const {
    for (const auto& c : cells) {
        if (c.key == key)
            return {true, c.value};
    }
    return {};
}

std::string_view org_row::value_or(std::string_view key,
    std::string_view fallback) const {
    const auto r = get(key);
    return r.found ? r.value : fallback;
}

std::vector<org_row> rows_of(const outline& o, std::uint32_t table) {
    std::vector<org_row> out;
    const auto first = first_row(o, table);
    const auto count = row_count(o, table);
    if (count == 0)
        return out;
    const auto headers = cell_count(o, first);
    for (std::uint32_t r = first + 1; r < first + count; ++r) {
        org_row row;
        const auto cells = cell_count(o, r);
        for (std::uint32_t i = 0; i < headers; ++i) {
            const auto header = cell(o, first, i);
            const auto value = i < cells ? cell(o, r, i) : std::string_view{};
            const auto it = std::ranges::find(row.cells, header,
                &key_value::key);
            if (it == row.cells.end())
                row.cells.push_back({header, value});
            else
                it->value = value;
        }
        out.push_back(std::move(row));
    }
    return out;
}

std::vector<org_row> own_table_rows(const outline& o, node_id n) {
    const auto tables = tables_of(o, n);
    if (tables.empty())
        return {};
    return rows_of(o, tables.front());
}

std::vector<org_row> first_table_rows(const outline& o, node_id n) {
    if (!tables_of(o, n).empty())
        return own_table_rows(o, n);
    for (const auto c : children(o, n)) {
        auto rows = first_table_rows(o, c);
        if (!rows.empty())
            return rows;
    }
    return {};
}

void set_properties(builder& b, value_id object, const outline& o, node_id n,
    std::initializer_list<std::string_view> raw) {
    for (const auto& p : properties(o, n)) {
        const auto key = to_lower(p.key);
        if (std::ranges::find(raw, std::string_view(key)) != raw.end())
            b.set(object, key, p.value);
        else
            set_typed(b, object, key, p.value);
    }
}

lookup org_id(const outline& o, node_id n) {
    lookup r;
    for (const auto& p : properties(o, n)) {
        if (iequals(p.key, "ID"))
            r = {true, p.value};
    }
    return r;
}

std::string pascal_case(std::string_view name) {
    std::string out;
    std::size_t start = 0;
    for (;;) {
        const auto end = name.find('_', start);
        const auto part = name.substr(start, end == std::string_view::npos
            ? std::string_view::npos : end - start);
        for (std::size_t i = 0; i < part.size(); ++i) {
            const char ch = part[i];
            if (i == 0)
                out += ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 32) : ch;
            else
                out += lower(ch);
        }
        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }
    return out;
}

}

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
#include "compass.codegen/ops/manifest.hpp"

#include <algorithm>
#include <cctype>

namespace compass::codegen::ops {

namespace {

constexpr std::string_view codegen_types[] = {
    "ores.codegen.entity", "ores.codegen.field_group",
    "ores.codegen.junction", "ores.codegen.lookup_entity",
    "ores.codegen.operation", "ores.codegen.family",
    "ores.codegen.service_registry", "ores.codegen.component",
    "ores.codegen.asset_class_catalogue", "ores.codegen.trade_type_catalogue",
    "ores.codegen.outcome_catalogue"};

std::string_view strip(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

std::vector<std::string> cells(std::string_view row) {
    row = strip(row);
    while (!row.empty() && row.front() == '|')
        row.remove_prefix(1);
    while (!row.empty() && row.back() == '|')
        row.remove_suffix(1);
    std::vector<std::string> out;
    std::size_t start = 0;
    while (true) {
        const auto end = row.find('|', start);
        out.emplace_back(strip(row.substr(start,
            end == std::string_view::npos ? end : end - start)));
        if (end == std::string_view::npos)
            return out;
        start = end + 1;
    }
}

bool is_rule(std::string_view row) {
    row = strip(row);
    if (row.size() < 2 || row.front() != '|')
        return false;
    return std::ranges::all_of(row, [](char c) {
        return c == '|' || c == '-' || c == '+';
    });
}

std::string_view parent(std::string_view path) {
    const auto slash = path.rfind('/');
    return slash == std::string_view::npos ? "" : path.substr(0, slash);
}

}

std::vector<catalogue_component> read_component_catalogue(
    std::string_view org) {
    std::vector<std::string_view> table;
    bool in_section = false;
    bool in_table = false;
    std::size_t start = 0;
    while (start <= org.size()) {
        auto end = org.find('\n', start);
        if (end == std::string_view::npos)
            end = org.size();
        const auto line = org.substr(start, end - start);
        start = end + 1;
        const auto s = strip(line);
        if (s == "* Components") {
            in_section = true;
            continue;
        }
        if (in_section && s.starts_with("* "))
            break;
        if (!in_table) {
            if (in_section && s.starts_with("| name")) {
                in_table = true;
                table.push_back(line);
            }
        } else if (s.starts_with("|")) {
            table.push_back(line);
        } else {
            break;
        }
    }

    std::vector<std::vector<std::string>> rows;
    for (const auto row : table)
        if (!is_rule(row))
            rows.push_back(cells(row));
    std::vector<catalogue_component> out;
    if (rows.size() < 2)
        return out;
    std::vector<std::string> headers;
    for (const auto& h : rows[0]) {
        std::string l(h);
        for (auto& c : l)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        headers.push_back(l);
    }
    const auto column = [&](const std::vector<std::string>& row,
        std::string_view name) -> std::string {
        for (std::size_t i = 0; i < headers.size() && i < row.size(); ++i)
            if (headers[i] == name)
                return row[i];
        return {};
    };
    for (std::size_t r = 1; r < rows.size(); ++r) {
        catalogue_component c;
        c.name = column(rows[r], "name");
        c.modeling_dir = column(rows[r], "modeling_dir");
        const auto excluded = column(rows[r], "exclude_org_types");
        std::string_view rest = excluded;
        while (!rest.empty()) {
            const auto comma = rest.find(',');
            const auto t = strip(rest.substr(0, comma));
            if (!t.empty())
                c.excluded_types.emplace_back(t);
            rest = comma == std::string_view::npos ? std::string_view()
                : rest.substr(comma + 1);
        }
        out.push_back(std::move(c));
    }
    return out;
}

std::set<std::string, std::less<>> regenerated_models(
    std::span<const catalogue_component> components,
    std::span<const std::string_view> paths,
    const std::function<std::string(std::string_view path)>& type_of) {
    std::set<std::string, std::less<>> out;
    for (const auto& c : components) {
        if (c.modeling_dir.empty())
            continue;
        const std::string_view dir = c.modeling_dir;
        const auto root = parent(dir);
        const bool exists = std::ranges::any_of(paths,
            [&](std::string_view p) { return parent(p) == dir; });
        if (!exists)
            continue;
        for (const std::string_view path : paths) {
            if (!path.ends_with(".org"))
                continue;
            if (parent(path) == dir) {
                const auto type = type_of(path);
                if (std::ranges::find(codegen_types, type) ==
                    std::end(codegen_types))
                    continue;
                const auto short_type = std::string_view(type).substr(13);
                if (std::ranges::find(c.excluded_types, short_type) !=
                    c.excluded_types.end())
                    continue;
                out.emplace(path);
            } else if (path.ends_with("/modeling/component_overview.org")) {
                if (parent(parent(parent(path))) == root)
                    out.emplace(path);
            }
        }
    }
    return out;
}

}

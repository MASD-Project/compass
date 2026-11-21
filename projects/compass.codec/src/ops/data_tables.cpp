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
#include "compass.codec/ops/data_tables.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>

namespace compass::codec::ops {

namespace {

bool same_name(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) ==
            std::tolower(static_cast<unsigned char>(y));
    });
}

void put_cell(std::string& out, std::string_view cell) {
    out.push_back('\t');
    for (const char c : cell) {
        if (c == '\\')
            out += "\\\\";
        else if (c == '\t')
            out += "\\t";
        else if (c == '\n')
            out += "\\n";
        else
            out.push_back(c);
    }
}

/**
 * @brief The cells of @p line after its first field, unescaped.
 */
std::vector<std::string> cells_of(std::string_view line) {
    std::vector<std::string> out;
    auto at = line.find('\t');
    while (at != std::string_view::npos) {
        std::string cell;
        auto i = at + 1;
        for (; i < line.size() && line[i] != '\t'; ++i) {
            if (line[i] == '\\' && i + 1 < line.size()) {
                const char n = line[++i];
                cell.push_back(n == 't' ? '\t' : n == 'n' ? '\n' : n);
            } else {
                cell.push_back(line[i]);
            }
        }
        out.push_back(std::move(cell));
        at = i < line.size() ? i : std::string_view::npos;
    }
    return out;
}

std::uint32_t number_of(std::string_view s) {
    std::uint32_t n = 0;
    std::from_chars(s.data(), s.data() + s.size(), n);
    return n;
}

}

std::size_t data_table::column(std::string_view column) const {
    const auto it = std::ranges::find_if(columns, [&](const auto& c) {
        return same_name(c, column);
    });
    return static_cast<std::size_t>(it - columns.begin());
}

const data_table* data_tables::find(std::string_view name) const {
    const auto it = std::ranges::find_if(tables, [&](const auto& t) {
        return same_name(t.name, name);
    });
    return it == tables.end() ? nullptr : &*it;
}

std::string encode_data_tables(const data_tables& tables) {
    std::string out = "SOURCE " + tables.source + "\n";
    for (const auto& t : tables.tables) {
        out += std::format("TABLE {}", t.line);
        put_cell(out, t.name);
        out += "\nCOLUMNS";
        for (const auto& c : t.columns)
            put_cell(out, c);
        out.push_back('\n');
        for (std::size_t r = 0; r < t.rows.size(); ++r) {
            out += std::format("ROW {}", t.row_lines[r]);
            for (const auto& c : t.rows[r])
                put_cell(out, c);
            out.push_back('\n');
        }
    }
    return out;
}

std::expected<data_tables, std::string> decode_data_tables(
    std::string_view text) {
    data_tables out;
    std::size_t at = 0;
    while (at < text.size()) {
        const auto end = std::min(text.find('\n', at), text.size());
        const auto line = text.substr(at, end - at);
        at = end + 1;
        if (line.starts_with("SOURCE ")) {
            out.source = std::string(line.substr(7));
        } else if (line.starts_with("TABLE ")) {
            auto cells = cells_of(line);
            if (cells.size() != 1)
                return std::unexpected("a TABLE line names no table");
            out.tables.push_back({.name = std::move(cells[0]),
                .line = number_of(line.substr(6))});
        } else if (line.starts_with("COLUMNS") && !out.tables.empty()) {
            out.tables.back().columns = cells_of(line);
        } else if (line.starts_with("ROW ") && !out.tables.empty()) {
            auto& t = out.tables.back();
            t.rows.push_back(cells_of(line));
            t.row_lines.push_back(number_of(line.substr(4)));
        } else if (!line.empty()) {
            return std::unexpected(std::format("'{}' is not a line of data "
                "tables", line.substr(0, 40)));
        }
    }
    return out;
}

std::expected<data_tables, std::string> tables_for(std::string_view codec,
    std::string_view text, std::span<const table_need> needed) {
    if (text.empty())
        return std::unexpected(std::format("{} reads its data from a check "
            "document, and none gives it: write compass/checks/{}.org with "
            ":CODEC: {} in the drawer of its * Definition heading, and a "
            "table under a level-1 heading for each of: {}", codec, codec,
            codec, [&] {
                std::string names;
                for (const auto& n : needed)
                    names += (names.empty() ? "" : ", ") + std::string(n.name);
                return names;
            }()));
    auto tables = decode_data_tables(text);
    if (!tables)
        return tables;
    for (const auto& n : needed) {
        const auto* t = tables->find(n.name);
        if (t == nullptr)
            return std::unexpected(std::format("{} has no table under the "
                "level-1 heading \"* {}\", which {} reads", tables->source,
                n.name, codec));
        for (const auto c : n.columns) {
            if (t->column(c) == t->columns.size())
                return std::unexpected(std::format("{}:{}: the table \"{}\" "
                    "has no column {}, which {} reads", tables->source,
                    t->line, n.name, c, codec));
        }
        for (std::size_t r = 0; r < t->rows.size(); ++r) {
            if (t->rows[r].size() != t->columns.size())
                return std::unexpected(std::format("{}:{}: the row has {} "
                    "cells, and the table \"{}\" {} columns", tables->source,
                    t->row_lines[r], t->rows[r].size(), n.name,
                    t->columns.size()));
        }
    }
    return tables;
}

}

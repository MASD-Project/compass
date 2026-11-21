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
#include "compass.env/ops/versions.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <optional>

namespace compass::env::ops {

namespace {

using failure = std::unexpected<std::string>;

std::string_view trim(std::string_view s) {
    const auto b = s.find_first_not_of(" \t");
    if (b == std::string_view::npos)
        return {};
    const auto e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

bool same_name(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) ==
            std::tolower(static_cast<unsigned char>(y));
    });
}

std::size_t width_of(std::string_view s) {
    return static_cast<std::size_t>(std::ranges::count_if(s, [](char c) {
        return (static_cast<unsigned char>(c) & 0xC0) != 0x80;
    }));
}

/**
 * @brief A cell as data: without the = or ~ that mark it verbatim.
 */
std::string_view unmarked(std::string_view cell) {
    if (cell.size() >= 2 && (cell.front() == '=' || cell.front() == '~') &&
        cell.back() == cell.front())
        return cell.substr(1, cell.size() - 2);
    return cell;
}

/**
 * @brief An org table in the lines of a document: its rows, a rule as no
 * cells, and the widths its first line aligns to.
 */
struct table {
    std::size_t first = 0;
    std::size_t end = 0;
    std::vector<std::optional<std::vector<std::string>>> rows;
    std::vector<bool> touched;
    std::vector<std::size_t> widths;

    [[nodiscard]] std::size_t column(std::string_view name) const {
        const auto& header = *rows.front();
        const auto it = std::ranges::find_if(header, [&](const auto& c) {
            return same_name(c, name);
        });
        return static_cast<std::size_t>(it - header.begin());
    }

    void set(std::size_t row, std::size_t column, std::string value) {
        auto& cells = *rows[row];
        if (cells.size() <= column)
            cells.resize(column + 1);
        cells[column] = std::move(value);
        touched[row] = true;
    }

    void append(std::vector<std::string> cells) {
        rows.emplace_back(std::move(cells));
        touched.push_back(true);
    }
};

std::vector<std::string> lines_of(std::string_view text) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < text.size()) {
        const auto end = std::min(text.find('\n', at), text.size());
        out.emplace_back(text.substr(at, end - at));
        at = end + 1;
    }
    return out;
}

std::vector<std::string> cells_of(std::string_view line) {
    auto s = trim(line);
    s.remove_prefix(1);
    if (s.ends_with('|'))
        s.remove_suffix(1);
    std::vector<std::string> out;
    std::size_t at = 0;
    for (;;) {
        const auto bar = s.find('|', at);
        out.emplace_back(trim(s.substr(at, bar - at)));
        if (bar == std::string_view::npos)
            break;
        at = bar + 1;
    }
    return out;
}

/**
 * @brief The first table under the level-1 heading @p title.
 */
std::optional<table> table_under(const std::vector<std::string>& lines,
    std::string_view title) {
    std::size_t i = 0;
    for (; i < lines.size(); ++i)
        if (lines[i].starts_with("* ") &&
            same_name(trim(std::string_view(lines[i]).substr(2)), title))
            break;
    for (++i; i < lines.size() && !lines[i].starts_with("* "); ++i)
        if (trim(lines[i]).starts_with('|'))
            break;
    if (i >= lines.size() || lines[i].starts_with("* "))
        return std::nullopt;
    table t{.first = i};
    for (; i < lines.size() && trim(lines[i]).starts_with('|'); ++i) {
        const auto line = trim(lines[i]);
        if (line.starts_with("|-"))
            t.rows.emplace_back(std::nullopt);
        else
            t.rows.emplace_back(cells_of(line));
        t.touched.push_back(false);
    }
    t.end = i;
    if (!t.rows.front())
        return std::nullopt;
    // The widths are those of the header line's segments, less the blank
    // at either end of each.
    auto header = trim(lines[t.first]);
    header.remove_prefix(1);
    if (header.ends_with('|'))
        header.remove_suffix(1);
    std::size_t at = 0;
    for (;;) {
        const auto bar = header.find('|', at);
        const auto w = width_of(header.substr(at, bar - at));
        t.widths.push_back(w >= 2 ? w - 2 : 0);
        if (bar == std::string_view::npos)
            break;
        at = bar + 1;
    }
    return t;
}

std::string render_row(const std::vector<std::string>& cells,
    const std::vector<std::size_t>& widths) {
    std::string out = "|";
    for (std::size_t c = 0; c < widths.size(); ++c) {
        const auto& cell = c < cells.size() ? cells[c] : std::string{};
        out += " " + cell + std::string(widths[c] - width_of(cell), ' ') +
            " |";
    }
    return out;
}

std::string render_rule(const std::vector<std::size_t>& widths) {
    std::string out = "|";
    for (std::size_t c = 0; c < widths.size(); ++c)
        out += std::string(widths[c] + 2, '-') + (c + 1 < widths.size()
            ? "+" : "|");
    return out;
}

/**
 * @brief The lines of @p t: the rows it touched rendered at its widths, or
 * every row at new widths when a touched cell does not fit.
 */
std::vector<std::string> render(const table& t,
    const std::vector<std::string>& lines) {
    bool fits = true;
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
        if (!t.touched[r] || !t.rows[r])
            continue;
        const auto& cells = *t.rows[r];
        for (std::size_t c = 0; c < cells.size(); ++c)
            if (c >= t.widths.size() || width_of(cells[c]) > t.widths[c])
                fits = false;
    }
    auto widths = t.widths;
    if (!fits) {
        widths.assign(t.rows.front()->size(), 0);
        for (const auto& row : t.rows)
            if (row)
                for (std::size_t c = 0; c < row->size() && c < widths.size();
                     ++c)
                    widths[c] = std::max(widths[c], width_of((*row)[c]));
    }
    std::vector<std::string> out;
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
        const auto source = t.first + r;
        if (fits && !t.touched[r] && source < t.end) {
            out.push_back(lines[source]);
            continue;
        }
        out.push_back(t.rows[r] ? render_row(*t.rows[r], widths)
                                : render_rule(widths));
    }
    return out;
}

std::string escaped(std::string_view s) {
    std::string out;
    for (const char c : s) {
        if (c == '|')
            out += "\\vert{}";
        else if (c != '\n' && c != '\r')
            out.push_back(c);
    }
    return out;
}

std::string joined(const std::vector<std::string>& names) {
    std::string out;
    for (const auto& n : names)
        out += (out.empty() ? "" : ", ") + n;
    return out;
}

}

std::expected<std::string, std::string> append_version(
    std::string_view document, const types::env_format& f,
    const version_request& r) {
    if (trim(r.description).empty())
        return failure("a version needs a description");
    if (r.adds.empty() && r.removes.empty() && !r.format_only)
        return failure("the version adds and removes no variable; name them "
            "with --adds and --removes, or pass --format-only when it "
            "changes only values, layout or the services the registry "
            "expands");
    const auto lines = lines_of(document);
    auto versions = table_under(lines, "Versions");
    auto variables = table_under(lines, "Variables");
    if (!versions || !variables)
        return failure(std::format("{} has no {} table", f.source,
            versions ? "Variables" : "Versions"));
    const auto number = f.current() + 1;

    const auto name_column = variables->column("variable");
    for (const auto& [names, column_name] : {
             std::pair{&r.adds, std::string_view("added")},
             std::pair{&r.removes, std::string_view("removed")}}) {
        const auto column = variables->column(column_name);
        const auto removed = variables->column("removed");
        for (const auto& name : *names) {
            bool found = false;
            for (std::size_t row = 1; row < variables->rows.size(); ++row) {
                const auto& cells = variables->rows[row];
                if (!cells || name_column >= cells->size() ||
                    unmarked((*cells)[name_column]) != name)
                    continue;
                const bool live = removed >= cells->size() ||
                    trim((*cells)[removed]).empty();
                if (column_name == "removed" && !live)
                    continue;
                variables->set(row, column, std::to_string(number));
                found = true;
            }
            if (!found)
                return failure(std::format("no {}row of the Variables table "
                    "is named {}; add its row first", column_name == "removed"
                    ? "live " : "", name));
        }
    }

    std::vector<std::string> cells(versions->rows.front()->size());
    const auto put = [&](std::string_view column, std::string value) {
        const auto c = versions->column(column);
        if (c < cells.size())
            cells[c] = std::move(value);
    };
    put("version", std::to_string(number));
    put("date", r.date);
    put("adds", joined(r.adds));
    put("removes", joined(r.removes));
    put("format only", r.format_only ? "yes" : "");
    put("description", escaped(trim(r.description)));
    versions->append(std::move(cells));

    std::vector<std::string> out;
    for (std::size_t i = 0; i < lines.size();) {
        const table* t = i == versions->first ? &*versions
            : i == variables->first ? &*variables : nullptr;
        if (t == nullptr) {
            out.push_back(lines[i++]);
            continue;
        }
        for (auto& line : render(*t, lines))
            out.push_back(std::move(line));
        i = t->end;
    }
    std::string text;
    for (const auto& line : out)
        text += line + "\n";
    if (!document.ends_with('\n') && !text.empty())
        text.pop_back();
    return text;
}

}

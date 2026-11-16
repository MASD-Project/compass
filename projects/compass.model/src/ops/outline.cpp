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
#include "compass.model/ops/outline.hpp"

#include <algorithm>
#include "text.hpp"

namespace compass::model::ops {

namespace {

using compass::org::types::element_id;
using compass::org::types::element_kind;

enum class line_class : std::uint8_t {
    text,
    heading,
    src_begin,
    src_body,
    src_end,
    comment,
    drawer_begin,
    drawer_line,
    drawer_end
};

bool is_begin_src(std::string_view line) {
    const auto rest = after_keyword(line, "#+begin_src");
    return rest && !rest->empty() && is_space(rest->front()) &&
        !trim(*rest).empty();
}

bool is_end_src(std::string_view line) {
    const auto rest = after_keyword(line, "#+end_src");
    return rest && trim(*rest).empty();
}

bool is_bare(std::string_view line, std::string_view keyword) {
    const auto rest = after_keyword(line, keyword);
    return rest && trim(*rest).empty();
}

std::uint32_t line_of(const std::vector<std::uint32_t>& starts,
    std::uint32_t offset) {
    const auto it = std::ranges::upper_bound(starts, offset);
    return static_cast<std::uint32_t>(it - starts.begin()) - 1;
}

/**
 * @brief Classifies the lines the heading, source block, comment block and
 * property drawer elements of @p t cover; every other line is text.
 */
void classify(const compass::org::types::tables_view& t,
    const std::vector<std::string_view>& lines,
    const std::vector<std::uint32_t>& starts, std::vector<line_class>& out) {
    out.assign(lines.size(), line_class::text);
    if (lines.empty())
        return;
    for (std::uint32_t e = 0; e < t.elements.size(); ++e) {
        const element_id id{e};
        const auto kind = t.elements.kinds[id];
        const auto range = t.elements.ranges[id];
        if (range.empty())
            continue;
        const auto first = line_of(starts, range.begin);
        const auto last = line_of(starts, range.end - 1);
        if (kind == element_kind::heading || kind == element_kind::inlinetask) {
            if (heading_title(lines[first]))
                out[first] = line_class::heading;
            continue;
        }
        if (kind == element_kind::src_block) {
            auto b = first;
            while (b <= last && !is_begin_src(lines[b]))
                ++b;
            auto end = last;
            while (end > b && !is_end_src(lines[end]))
                --end;
            if (b > last || end == b)
                continue;
            out[b] = line_class::src_begin;
            for (auto l = b + 1; l < end; ++l)
                out[l] = line_class::src_body;
            out[end] = line_class::src_end;
            continue;
        }
        if (kind == element_kind::comment_block) {
            auto b = first;
            while (b <= last && !is_bare(lines[b], "#+begin_comment"))
                ++b;
            auto end = last;
            while (end > b && !is_bare(lines[end], "#+end_comment"))
                --end;
            if (b > last || end == b)
                continue;
            for (auto l = b; l <= end; ++l)
                out[l] = line_class::comment;
            continue;
        }
        if (kind == element_kind::property_drawer ||
            kind == element_kind::drawer) {
            if (!iequals(trim(lines[first]), ":PROPERTIES:"))
                continue;
            auto end = last;
            while (end > first && !iequals(trim(lines[end]), ":END:"))
                --end;
            if (end == first)
                continue;
            out[first] = line_class::drawer_begin;
            for (auto l = first + 1; l < end; ++l)
                out[l] = line_class::drawer_line;
            out[end] = line_class::drawer_end;
        }
    }
}

}

void build_outline(std::string_view source,
    const compass::org::types::tables_view& t, outline& o) {
    o = outline{};
    std::vector<std::string_view> lines;
    std::vector<std::uint32_t> starts;
    split_lines(source, lines, starts);
    std::vector<line_class> classes;
    classify(t, lines, starts, classes);

    o.levels.push_back(0);
    o.titles.emplace_back();
    o.body_firsts.push_back(0);
    node_id current{0};
    bool seen_heading = false;

    std::vector<std::string_view> block;
    std::string_view block_name, block_language, block_implements;
    std::vector<std::vector<std::string_view>> pending_table;
    std::vector<std::string_view> bullets;

    const auto close_table = [&] {
        if (pending_table.empty())
            return;
        o.table_nodes.push_back(current);
        o.table_row_firsts.push_back(
            static_cast<std::uint32_t>(o.row_cell_firsts.size()));
        for (const auto& row : pending_table) {
            o.row_cell_firsts.push_back(
                static_cast<std::uint32_t>(o.cells.size()));
            o.cells.insert(o.cells.end(), row.begin(), row.end());
        }
        pending_table.clear();
    };
    const auto flush_bullets = [&] {
        if (bullets.empty())
            return;
        o.list_nodes.push_back(current);
        o.list_item_firsts.push_back(
            static_cast<std::uint32_t>(o.list_items.size()));
        o.list_items.insert(o.list_items.end(), bullets.begin(),
            bullets.end());
        bullets.clear();
    };

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto line = lines[i];
        switch (classes[i]) {
        case line_class::src_begin: {
            close_table();
            flush_bullets();
            const auto header = src_header(line);
            block_language = header.language;
            block_name = header_argument(header.rest, ":name");
            block_implements = header_argument(header.rest, ":implements");
            block.clear();
            continue;
        }
        case line_class::src_body:
            block.push_back(line);
            continue;
        case line_class::src_end: {
            std::string_view code;
            if (!block.empty()) {
                code = std::string_view(block.front().data(),
                    static_cast<std::size_t>(block.back().data() +
                        block.back().size() - block.front().data()));
            }
            o.block_nodes.push_back(current);
            o.block_names.push_back(block_name);
            o.block_languages.push_back(block_language);
            o.block_implements.push_back(block_implements);
            o.block_codes.push_back(code);
            continue;
        }
        case line_class::comment:
            if (is_bare(line, "#+begin_comment")) {
                close_table();
                flush_bullets();
            }
            continue;
        case line_class::drawer_begin:
        case line_class::drawer_end:
            continue;
        case line_class::drawer_line: {
            const auto p = drawer_property(line);
            if (!p)
                continue;
            if (!seen_heading && current.value == 0) {
                o.file_property_keys.push_back(p->key);
                o.file_property_values.push_back(p->value);
            } else {
                o.property_nodes.push_back(current);
                o.property_keys.push_back(p->key);
                o.property_values.push_back(p->value);
            }
            continue;
        }
        case line_class::heading: {
            close_table();
            flush_bullets();
            seen_heading = true;
            const auto title = heading_title(line);
            current = node_id{static_cast<std::uint32_t>(o.levels.size())};
            o.levels.push_back(static_cast<std::uint16_t>(title->level));
            o.titles.push_back(title->title);
            o.body_firsts.push_back(
                static_cast<std::uint32_t>(o.body_lines.size()));
            continue;
        }
        case line_class::text:
            break;
        }
        if (!seen_heading) {
            if (const auto kw = frontmatter_line(line)) {
                o.frontmatter_keys.push_back(kw->key);
                o.frontmatter_values.push_back(kw->value);
                continue;
            }
        }
        if (is_table_rule(line))
            continue;
        if (const auto row = table_row(line)) {
            pending_table.push_back(*row);
            continue;
        }
        close_table();
        if (const auto item = bullet(line)) {
            bullets.push_back(*item);
            o.body_lines.push_back(line);
            continue;
        }
        flush_bullets();
        o.body_lines.push_back(line);
    }
    close_table();
    flush_bullets();

    const auto n = o.levels.size();
    o.subtree_ends.assign(n, node_id{static_cast<std::uint32_t>(n)});
    std::vector<std::uint32_t> open;
    for (std::uint32_t i = 0; i < n; ++i) {
        while (!open.empty() && o.levels[open.back()] >= o.levels[i]) {
            o.subtree_ends[open.back()] = node_id{i};
            open.pop_back();
        }
        open.push_back(i);
    }
}

std::vector<node_id> children(const outline& o, node_id n) {
    std::vector<node_id> out;
    if (n.is_none())
        return out;
    const auto end = o.subtree_ends[n.value];
    for (node_id c{n.value + 1}; c < end; c = o.subtree_ends[c.value])
        out.push_back(c);
    return out;
}

node_id section(const outline& o, node_id n, std::string_view title) {
    for (const auto c : children(o, n)) {
        if (iequals(o.titles[c.value], title))
            return c;
    }
    return node_id::none();
}

namespace {

lookup last_of(const std::vector<std::string_view>& keys,
    const std::vector<std::string_view>& values, std::string_view key) {
    lookup r;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == key) {
            r.found = true;
            r.value = values[i];
        }
    }
    return r;
}

}

lookup property(const outline& o, node_id n, std::string_view key) {
    lookup r;
    if (n.is_none())
        return r;
    for (std::size_t i = 0; i < o.property_keys.size(); ++i) {
        if (o.property_nodes[i] == n && o.property_keys[i] == key) {
            r.found = true;
            r.value = o.property_values[i];
        }
    }
    return r;
}

lookup frontmatter(const outline& o, std::string_view key) {
    return last_of(o.frontmatter_keys, o.frontmatter_values, key);
}

lookup file_property(const outline& o, std::string_view key) {
    return last_of(o.file_property_keys, o.file_property_values, key);
}

std::vector<key_value> properties(const outline& o, node_id n) {
    std::vector<key_value> out;
    if (n.is_none())
        return out;
    for (std::size_t i = 0; i < o.property_keys.size(); ++i) {
        if (o.property_nodes[i] != n)
            continue;
        const auto key = o.property_keys[i];
        const auto it = std::ranges::find(out, key, &key_value::key);
        if (it == out.end())
            out.push_back({key, o.property_values[i]});
        else
            it->value = o.property_values[i];
    }
    return out;
}

lookup named_block(const outline& o, node_id n, std::string_view name) {
    lookup r;
    if (n.is_none())
        return r;
    for (std::size_t i = 0; i < o.block_nodes.size(); ++i) {
        if (o.block_nodes[i] == n && !o.block_names[i].empty() &&
            o.block_names[i] == name) {
            r.found = true;
            r.value = o.block_codes[i];
        }
    }
    return r;
}

std::string strip_markup(std::string_view text) {
    return strip_verbatim(text);
}

std::string body(const outline& o, node_id n, bool per_line) {
    if (n.is_none())
        return {};
    const auto first = o.body_firsts[n.value];
    const auto end = n.value + 1 < o.body_firsts.size()
        ? o.body_firsts[n.value + 1]
        : static_cast<std::uint32_t>(o.body_lines.size());
    auto b = first;
    auto e = end;
    while (b < e && trim(o.body_lines[b]).empty())
        ++b;
    while (e > b && trim(o.body_lines[e - 1]).empty())
        --e;
    std::string out;
    if (per_line) {
        for (auto i = b; i < e; ++i) {
            if (i != b)
                out += '\n';
            out += strip_verbatim(o.body_lines[i]);
        }
        return out;
    }
    for (auto i = b; i < e; ++i) {
        if (i != b)
            out += '\n';
        out += o.body_lines[i];
    }
    return strip_verbatim(out);
}

std::vector<std::uint32_t> tables_of(const outline& o, node_id n) {
    std::vector<std::uint32_t> out;
    for (std::uint32_t i = 0; i < o.table_nodes.size(); ++i) {
        if (o.table_nodes[i] == n)
            out.push_back(i);
    }
    return out;
}

std::uint32_t first_row(const outline& o, std::uint32_t table) {
    return o.table_row_firsts[table];
}

std::uint32_t row_count(const outline& o, std::uint32_t table) {
    const auto end = table + 1 < o.table_row_firsts.size()
        ? o.table_row_firsts[table + 1]
        : static_cast<std::uint32_t>(o.row_cell_firsts.size());
    return end - o.table_row_firsts[table];
}

std::uint32_t cell_count(const outline& o, std::uint32_t row) {
    const auto end = row + 1 < o.row_cell_firsts.size()
        ? o.row_cell_firsts[row + 1]
        : static_cast<std::uint32_t>(o.cells.size());
    return end - o.row_cell_firsts[row];
}

std::string_view cell(const outline& o, std::uint32_t row, std::uint32_t i) {
    return o.cells[o.row_cell_firsts[row] + i];
}

}

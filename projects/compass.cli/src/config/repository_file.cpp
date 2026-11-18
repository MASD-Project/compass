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
#include "compass.cli/config/repository_file.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>
#include "compass.codec/ops/exceptions.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::cli::config {

namespace {

namespace ot = compass::org::types;
using compass::org::ops::trimmed;

using failure = std::unexpected<std::string>;

/**
 * @brief @p cell without the = or ~ that mark it verbatim in org.
 */
std::string_view verbatim(std::string_view cell) {
    for (const char mark : {'=', '~'})
        if (cell.size() >= 2 && cell.front() == mark && cell.back() == mark)
            return cell.substr(1, cell.size() - 2);
    return cell;
}

std::uint32_t line_of(std::string_view text, std::uint32_t offset) {
    return static_cast<std::uint32_t>(
        1 + std::ranges::count(text.substr(0, offset), '\n'));
}

std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") + 1 - first);
}

/**
 * @brief Reads the document after parsing: knows the text, the tables
 * and the label errors start with.
 */
struct reader {
    std::string_view text;
    ot::tables_view t;
    std::string_view label;

    [[nodiscard]] failure error(std::uint32_t offset,
        std::string_view what, std::string_view fix) const {
        return failure(std::format("{}:{}: {}; fix: {}", label,
            line_of(text, offset), what, fix));
    }

    [[nodiscard]] std::expected<std::optional<types::sourced_path>,
        std::string> path(std::string_view key) const {
        const auto k = compass::org::ops::find_keyword(t, key);
        if (k.is_none())
            return std::nullopt;
        const auto value = trimmed(text, t.keywords.values[k]);
        const auto begin = t.elements.ranges[t.keywords.elements[k]].begin;
        if (value.empty())
            return error(begin, std::format("#+{}: has no path", key),
                std::format("write a path after #+{}:, relative to the "
                            "checkout", key));
        return types::sourced_path{std::string(value.of(text)),
            {types::origin_kind::file, line_of(text, begin)}};
    }

    /**
     * @brief The first table under the level-1 heading @p title, or none.
     */
    [[nodiscard]] ot::element_id table_under(std::string_view title) const {
        for (std::uint32_t h = 0; h < t.headings.size(); ++h) {
            const ot::heading_id id{h};
            if (t.headings.levels[id] != 1 ||
                trimmed(text, t.headings.titles[id]).of(text) != title)
                continue;
            const auto heading = t.headings.elements[id];
            for (auto e = heading.next();
                 e.value < t.elements.subtree_ends[heading].value;
                 e = e.next()) {
                if (t.elements.kinds[e] == ot::element_kind::table)
                    return e;
            }
        }
        return ot::element_id::none();
    }

    /**
     * @brief The text of each cell of @p row, trimmed, without its bar.
     */
    [[nodiscard]] std::vector<std::string_view> cells(ot::element_id row) const {
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

    /**
     * @brief The rows of @p table but a first row whose first cell is
     * @p header, in order.
     */
    [[nodiscard]] std::vector<ot::element_id> rows(ot::element_id table,
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

    void exception_row(ot::element_id row,
        codec::types::exception_set& out) const {
        const auto line = line_of(text, t.elements.ranges[row].begin);
        const auto c = cells(row);
        if (c.size() != 6) {
            out.malformed.push_back({std::string(repository_file_name), line,
                std::format("the row has {} cells, not 6; write | path | "
                    "line | rule | expires | approver | reason |",
                    c.size())});
            return;
        }
        codec::ops::add_exception_row(out, repository_file_name, line, {
            .path = verbatim(c[0]), .line = c[1], .rule = verbatim(c[2]),
            .expires = c[3], .approver = c[4], .reason = c[5]});
    }

    [[nodiscard]] std::expected<void, std::string> codec_row(
        ot::element_id row, types::layer& out) const {
        const auto begin = t.elements.ranges[row].begin;
        const auto line = line_of(text, begin);
        const auto c = cells(row);
        if (c.size() != 3)
            return error(begin, std::format("the row has {} cells, not 3",
                c.size()), "write | name | enabled | options |");
        const auto name = std::string(c[0]);
        if (name.empty())
            return error(begin, "the row names no codec",
                "write the codec's name in the first cell");
        const auto previous = std::ranges::find(out.settings, name,
            &types::setting::codec);
        if (previous != out.settings.end())
            return error(begin, std::format("codec '{}' is configured "
                "twice, first on line {}", name, previous->from.line),
                "keep one row per codec");
        if (c[1] != "yes" && c[1] != "no")
            return error(begin, std::format("enabled is '{}'", c[1]),
                "write yes or no");
        const types::origin from{types::origin_kind::file, line};
        out.settings.push_back({name, "enabled",
            c[1] == "yes" ? "true" : "false", from});

        auto rest = c[2];
        while (!(rest = trim(rest)).empty()) {
            const auto space = rest.find_first_of(" \t");
            const auto pair = rest.substr(0, space);
            rest = space == std::string_view::npos ? std::string_view{}
                                                   : rest.substr(space);
            const auto equals = pair.find('=');
            if (equals == std::string_view::npos || equals == 0)
                return error(begin, std::format("option '{}' is not "
                    "key=value", pair), "write each option as key=value, "
                    "separated by spaces");
            const auto key = pair.substr(0, equals);
            if (key == "enabled")
                return error(begin, "enabled is set among the options",
                    "use the enabled column");
            out.settings.push_back({name, std::string(key),
                std::string(pair.substr(equals + 1)), from});
        }
        return {};
    }
};

}

std::expected<types::layer, std::string> parse_repository_file(
    std::string_view text, std::string_view label) {
    ot::tables tables;
    ingest::org::parse(text, tables);
    const reader r{text, ot::view(tables), label};

    types::layer out;
    auto output = r.path("output");
    if (!output)
        return failure(output.error());
    out.output = *output;
    auto meta = r.path("meta");
    if (!meta)
        return failure(meta.error());
    out.meta = *meta;

    const auto table = r.table_under("Codecs");
    if (table.is_none())
        return failure(std::format("{}: no table under a level-1 heading "
            "\"* Codecs\"; fix: add the heading and a | name | enabled | "
            "options | table under it", label));
    for (const auto row : r.rows(table, "name"))
        if (auto ok = r.codec_row(row, out); !ok)
            return failure(ok.error());
    if (const auto exceptions = r.table_under("Exceptions");
        !exceptions.is_none())
        for (const auto row : r.rows(exceptions, "path"))
            r.exception_row(row, out.exceptions);
    return out;
}

std::expected<types::layer, std::string> read_repository_file(
    const std::filesystem::path& checkout) {
    const auto path = checkout / repository_file_name;
    std::ifstream is(path, std::ios::binary);
    if (!is) {
        if (!std::filesystem::exists(path))
            return types::layer{};
        return failure(std::format("{}: cannot be read; fix: make it "
            "readable", path.string()));
    }
    const std::string text{std::istreambuf_iterator<char>(is), {}};
    return parse_repository_file(text, repository_file_name);
}

std::expected<std::optional<types::layer>, std::string> read_repository_file(
    const store::io::git_objects& objects,
    const store::types::object_id& commit) {
    const auto text = store::io::read_commit_file(objects, commit,
        repository_file_name);
    if (!text)
        return std::nullopt;
    const auto label = store::ops::to_hex(commit).substr(0, 7) + ":" +
        std::string(repository_file_name);
    return parse_repository_file(*text, label);
}

}

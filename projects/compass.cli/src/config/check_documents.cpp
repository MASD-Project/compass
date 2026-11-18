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
#include "compass.cli/config/check_documents.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include "compass.cli/config/org_reader.hpp"
#include "compass.codec/ops/exceptions.hpp"
#include "compass.cpp/codecs/catalogue.hpp"
#include "compass.cpp/codecs/forbid.hpp"
#include "compass.cpp/ops/forbid.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/ops/document_fields.hpp"

namespace compass::cli::config {

namespace {

namespace ot = compass::org::types;
namespace ct = compass::codec::types;
using compass::org::ops::trimmed;

using failure = std::unexpected<std::string>;

/**
 * @brief The definition text a document's drawers make, with the line of
 * the document each of its lines came from.
 */
struct definition_text {
    std::string text;
    std::vector<std::uint32_t> lines;

    void add(std::string_view line, std::uint32_t from) {
        text.append(line).push_back('\n');
        lines.push_back(from);
    }
};

struct check_reader : org_reader {
    std::string_view path;

    [[nodiscard]] failure error(std::uint32_t line, std::string_view what,
        std::string_view fix) const {
        return failure(std::format("{}:{}: {}; fix: {}", path, line, what,
            fix));
    }

    [[nodiscard]] std::uint32_t line_of_heading(ot::heading_id h) const {
        return line_of(text, t.elements.ranges[t.headings.elements[h]].begin);
    }

    /**
     * @brief The rows of the property table that the drawer of @p h holds.
     */
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> properties_of(
        ot::heading_id h) const {
        const auto next = h.value + 1 < t.headings.size()
            ? t.headings.property_firsts[ot::heading_id{h.value + 1}].value
            : static_cast<std::uint32_t>(t.properties.size());
        return {t.headings.property_firsts[h].value, next};
    }

    /**
     * @brief The first paragraph of @p h before any heading under it, its
     * lines joined by single spaces, or nothing.
     */
    [[nodiscard]] std::string first_paragraph(ot::heading_id h) const {
        const auto e = t.headings.elements[h];
        for (auto i = e.next(); i.value < t.elements.subtree_ends[e].value;
             i = i.next()) {
            const auto kind = t.elements.kinds[i];
            if (kind == ot::element_kind::heading)
                break;
            if (kind != ot::element_kind::paragraph)
                continue;
            std::string out;
            for (const char c : trim(t.elements.ranges[i].of(text))) {
                const bool blank = c == ' ' || c == '\n' || c == '\t' ||
                    c == '\r';
                if (!blank)
                    out.push_back(c);
                else if (!out.empty() && out.back() != ' ')
                    out.push_back(' ');
            }
            return out;
        }
        return {};
    }

    /**
     * @brief Adds the drawer of @p h to @p out, one KEY value line per
     * property, or says why it cannot be: the engine is checked and left
     * out, and so is the drawer's ID.
     */
    [[nodiscard]] std::expected<void, std::string> add_drawer(
        ot::heading_id h, definition_text& out, bool& engine) const {
        const auto [first, end] = properties_of(h);
        for (auto p = first; p < end; ++p) {
            const ot::property_id id{p};
            const auto key = compass::org::ops::name_text(t,
                t.properties.names[id]);
            const auto value = trimmed(text, t.properties.values[id]);
            const auto line = line_of(text, value.begin);
            const auto v = value.of(text);
            if (key == "ENGINE") {
                if (v != "forbid")
                    return error(line, std::format("the engine is '{}'", v),
                        "write :ENGINE: forbid, the one engine compass "
                        "runs from a document");
                engine = true;
                continue;
            }
            if (key == "ID")
                continue;
            out.add(std::format("{} {}", key, v), line);
        }
        return {};
    }
};

}

std::expected<std::unique_ptr<const check>, std::string>
parse_check_document(std::string_view text, std::string_view path,
    ct::exception_set& exceptions) {
    ot::tables tables;
    ingest::org::parse(text, tables);
    const check_reader r{{text, ot::view(tables)}, path};

    auto c = std::make_unique<check>();
    const auto slash = path.rfind('/');
    auto file = path.substr(slash == std::string_view::npos ? 0 : slash + 1);
    if (file.ends_with(".org"))
        file.remove_suffix(4);
    c->name = std::string(file);
    c->path = std::string(path);

    const auto definition = r.heading("Definition");
    if (definition.is_none())
        return r.error(1, "the document has no level-1 heading "
            "\"* Definition\"", "add it, with :ENGINE: forbid and the "
            "check's FILES in its property drawer");
    definition_text d;
    bool engine = false;
    if (auto ok = r.add_drawer(definition, d, engine); !ok)
        return failure(ok.error());
    if (!engine)
        return r.error(r.line_of_heading(definition), "the definition names "
            "no engine", "add :ENGINE: forbid to its property drawer, which "
            "org reads only on the line right under the heading");

    std::vector<ot::heading_id> rule_headings;
    for (auto h = definition.value + 1; h < tables.headings.size() &&
         tables.headings.levels[ot::heading_id{h}] > 1; ++h) {
        if (tables.headings.levels[ot::heading_id{h}] == 2)
            rule_headings.push_back(ot::heading_id{h});
    }
    for (const auto h : rule_headings) {
        const auto name = trimmed(text, tables.headings.titles[h]).of(text);
        d.add(std::format("RULE {}", name), r.line_of_heading(h));
        if (auto ok = r.add_drawer(h, d, engine); !ok)
            return failure(ok.error());
    }

    auto parsed = compass::cpp::ops::parse_forbid_definition(d.text);
    if (!parsed) {
        const auto& e = parsed.error();
        std::uint32_t line = r.line_of_heading(definition);
        auto what = std::string_view(e);
        if (e.starts_with("line ")) {
            const auto colon = e.find(": ");
            const auto n = static_cast<std::size_t>(
                std::stoul(e.substr(5, colon - 5)));
            if (n >= 1 && n <= d.lines.size())
                line = d.lines[n - 1];
            what = what.substr(colon + 2);
        }
        return r.error(line, what, "correct the definition's drawers");
    }

    for (std::size_t i = 0; i < parsed->rules.size(); ++i) {
        c->rule_names.push_back(parsed->rules[i].name);
        auto summary = r.first_paragraph(rule_headings[i]);
        c->rule_summaries.push_back(summary.empty()
            ? parsed->rules[i].message : std::move(summary));
    }
    for (std::size_t i = 0; i < c->rule_names.size(); ++i)
        c->rules.push_back({c->rule_names[i], c->rule_summaries[i]});
    c->definition = std::move(d.text);
    c->options = {{{compass::cpp::codecs::forbid_definition_key,
        c->definition, "The check's definition, from its document."}}};

    const auto description = compass::org::ops::find_keyword(r.t,
        "description");
    c->summary = description.is_none() ? c->path : std::string(
        trimmed(text, r.t.keywords.values[description]).of(text));
    c->codec = {
        .name = c->name,
        .version = 1,
        .code = compass::cpp::codecs::code_hash(),
        .summary = c->summary,
        .reads = ct::inputs::of_types({}),
        .options = c->options,
        .rules = c->rules,
        .enabled_by_default = true,
        .run = compass::cpp::codecs::forbid_function()};

    if (const auto table = r.table_under("Exceptions"); !table.is_none()) {
        for (const auto row : r.rows(table, "path")) {
            const auto line = line_of(text, tables.elements.ranges[row].begin);
            const auto cells = r.cells(row);
            if (cells.size() != 6) {
                exceptions.malformed.push_back({std::string(path), line,
                    std::format("the row has {} cells, not 6; write | path "
                        "| line | rule | expires | approver | reason |",
                        cells.size())});
                continue;
            }
            const auto rule = c->name + "/" + std::string(verbatim(cells[2]));
            codec::ops::add_exception_row(exceptions, path, line, {
                .path = verbatim(cells[0]), .line = cells[1], .rule = rule,
                .expires = cells[3], .approver = cells[4],
                .reason = cells[5]});
        }
    }
    return c;
}

std::expected<check_set, std::string> read_check_documents(
    const std::filesystem::path& checkout) {
    check_set out;
    const auto directory = checkout / checks_directory;
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec))
        return out;
    std::vector<std::string> names;
    for (const auto& e : std::filesystem::directory_iterator(directory)) {
        if (e.is_regular_file() && e.path().extension() == ".org")
            names.push_back(e.path().filename().generic_string());
    }
    std::ranges::sort(names);
    for (const auto& n : names) {
        const auto path = std::string(checks_directory) + "/" + n;
        std::ifstream is(directory / n, std::ios::binary);
        if (!is)
            return failure(std::format("{}: cannot be read; fix: make it "
                "readable", path));
        const std::string text{std::istreambuf_iterator<char>(is), {}};
        auto c = parse_check_document(text, path, out.exceptions);
        if (!c)
            return failure(c.error());
        out.checks.push_back(std::move(*c));
    }
    return out;
}

std::expected<check_set, std::string> read_check_documents(
    const store::io::git_objects& objects,
    const store::types::object_id& commit) {
    check_set out;
    const auto prefix = std::string(checks_directory) + "/";
    auto files = store::io::list_commit(objects, commit);
    std::vector<std::string> paths;
    for (const auto& p : files.paths) {
        if (p.starts_with(prefix) && p.ends_with(".org") &&
            p.find('/', prefix.size()) == std::string::npos)
            paths.push_back(p);
    }
    std::ranges::sort(paths);
    for (const auto& path : paths) {
        const auto text = store::io::read_commit_file(objects, commit, path);
        if (!text)
            continue;
        auto c = parse_check_document(*text, path, out.exceptions);
        if (!c)
            return failure(c.error());
        out.checks.push_back(std::move(*c));
    }
    return out;
}

}

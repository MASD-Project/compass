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
#include "compass.db/ops/extract.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>
#include "compass.org/ops/document_fields.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::db::ops {

namespace {

namespace ot = compass::org::types;
using compass::org::ops::name_text;
using compass::org::ops::trimmed;

bool is_blank(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && is_blank(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && is_blank(s.back()))
        s.remove_suffix(1);
    return s;
}

void append_json_string(std::string& out, std::string_view s) {
    out += '"';
    for (const char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
                out += std::format("\\u{:04x}", static_cast<unsigned>(c));
            else
                out += c;
        }
    }
    out += '"';
}

/**
 * @brief Names with their values, in the order each name first appears,
 * written as a JSON object of strings or, with lists, of arrays.
 */
struct json_object {
    std::vector<std::pair<std::string_view, std::vector<std::string_view>>>
        entries;

    void add(std::string_view name, std::string_view value) {
        const auto it = std::ranges::find(entries, name,
            &decltype(entries)::value_type::first);
        if (it == entries.end())
            entries.push_back({name, {value}});
        else
            it->second.push_back(value);
    }

    [[nodiscard]] std::string text(bool lists) const {
        std::string out = "{";
        for (const auto& [name, values] : entries) {
            if (out.size() > 1)
                out += ',';
            append_json_string(out, name);
            out += ':';
            if (!lists) {
                append_json_string(out, values.back());
                continue;
            }
            out += '[';
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i != 0)
                    out += ',';
                append_json_string(out, values[i]);
            }
            out += ']';
        }
        return out + "}";
    }
};

/**
 * @brief Reads one parsed document: plain text of its ranges, and where
 * its headings and links sit.
 */
struct reader {
    std::string_view source;
    const ot::tables_view& t;
    std::vector<std::uint32_t> heading_begins;
    std::vector<std::uint32_t> link_begins;

    reader(std::string_view s, const ot::tables_view& tables)
        : source(s), t(tables) {
        heading_begins.reserve(t.headings.size());
        for (std::uint32_t h = 0; h < t.headings.size(); ++h)
            heading_begins.push_back(
                t.elements.ranges[t.headings.elements[ot::heading_id{h}]]
                    .begin);
        link_begins.reserve(t.links.size());
        for (std::uint32_t l = 0; l < t.links.size(); ++l)
            link_begins.push_back(link_range(ot::link_id{l}).begin);
    }

    /**
     * @brief The bytes of link @p l, without the blanks after it.
     */
    [[nodiscard]] ot::source_range link_range(ot::link_id l) const {
        const auto o = t.links.objects[l];
        auto r = t.objects.ranges[o];
        r.end -= std::min<std::uint32_t>(t.objects.post_blanks[o], r.size());
        return r;
    }

    [[nodiscard]] std::string_view text_of(ot::source_range r) const {
        return r.of(source);
    }

    /**
     * @brief What a link reads as: its description, else the link as
     * written without its brackets.
     */
    [[nodiscard]] std::string_view link_text(ot::link_id l) const {
        const auto description = t.objects.contents[t.links.objects[l]];
        if (!description.empty())
            return text_of(trimmed(source, description));
        auto written = text_of(link_range(l));
        if (written.starts_with("[[") && written.ends_with("]]"))
            return written.substr(2, written.size() - 4);
        if (written.starts_with('<') && written.ends_with('>'))
            return written.substr(1, written.size() - 2);
        return written;
    }

    /**
     * @brief The text of @p r with each link replaced by what it reads as.
     */
    void append_plain(std::string& out, ot::source_range r) const {
        auto at = r.begin;
        auto l = static_cast<std::uint32_t>(
            std::ranges::lower_bound(link_begins, r.begin) -
            link_begins.begin());
        for (; l < t.links.size(); ++l) {
            const ot::link_id id{l};
            const auto range = link_range(id);
            if (range.begin >= r.end)
                break;
            if (range.begin < at)
                continue;
            out += source.substr(at, range.begin - at);
            out += link_text(id);
            at = std::min(range.end, r.end);
        }
        if (at < r.end)
            out += source.substr(at, r.end - at);
    }

    [[nodiscard]] std::string plain(ot::source_range r) const {
        std::string out;
        append_plain(out, r);
        return std::string(trim(out));
    }

    /**
     * @brief The heading a byte at @p offset sits under, none before the
     * first heading.
     */
    [[nodiscard]] std::optional<std::uint32_t> heading_at(
        std::uint32_t offset) const {
        const auto it = std::ranges::upper_bound(heading_begins, offset);
        if (it == heading_begins.begin())
            return std::nullopt;
        return static_cast<std::uint32_t>(it - heading_begins.begin() - 1);
    }

    [[nodiscard]] std::string title(ot::heading_id h) const {
        return plain(t.headings.titles[h]);
    }

    /**
     * @brief The properties of the drawer of heading @p h, or of the
     * document before its first heading when @p h is none.
     */
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> property_run(
        ot::heading_id h) const {
        const auto count = static_cast<std::uint32_t>(t.properties.size());
        const auto first_of = [&](std::uint32_t heading) {
            return heading < t.headings.size()
                ? t.headings.property_firsts[ot::heading_id{heading}].value
                : count;
        };
        if (h.is_none())
            return {0, first_of(0)};
        return {first_of(h.value), first_of(h.value + 1)};
    }

    [[nodiscard]] std::string properties_json(ot::heading_id h,
        std::optional<std::string_view>* id = nullptr) const {
        json_object o;
        const auto [first, end] = property_run(h);
        for (auto p = first; p < end; ++p) {
            const ot::property_id pid{p};
            const auto name = name_text(t, t.properties.names[pid]);
            const auto value = text_of(t.properties.values[pid]);
            o.add(name, value);
            if (id != nullptr && name == "ID")
                *id = value;
        }
        return o.text(false);
    }

    /**
     * @brief The section element right under element @p e, if it has one.
     */
    [[nodiscard]] std::optional<ot::element_id> section_under(
        ot::element_id e) const {
        const ot::element_id next = e.next();
        if (next.value < t.elements.size() &&
            t.elements.kinds[next] == ot::element_kind::section &&
            t.elements.parents[next] == e)
            return next;
        return std::nullopt;
    }

    /**
     * @brief The prose of section @p s: every child element but drawers,
     * keywords, planning lines, clocks and comments, as plain text.
     */
    [[nodiscard]] std::string section_text(ot::element_id s) const {
        std::string out;
        const auto end = t.elements.subtree_ends[s];
        for (auto c = s.next(); c < end; c = t.elements.subtree_ends[c]) {
            switch (t.elements.kinds[c]) {
            case ot::element_kind::property_drawer:
            case ot::element_kind::drawer:
            case ot::element_kind::keyword:
            case ot::element_kind::babel_call:
            case ot::element_kind::planning:
            case ot::element_kind::clock:
            case ot::element_kind::comment:
            case ot::element_kind::comment_block:
                continue;
            default:
                append_plain(out, t.elements.ranges[c]);
            }
        }
        return std::string(trim(out));
    }

    [[nodiscard]] bool before_first_heading(ot::element_id e) const {
        return heading_begins.empty() ||
            t.elements.ranges[e].begin < heading_begins.front();
    }

    [[nodiscard]] std::optional<ot::heading_id> top_heading(
        std::string_view title_text) const {
        for (std::uint32_t h = 0; h < t.headings.size(); ++h) {
            const ot::heading_id id{h};
            if (t.headings.levels[id] == 1 && title(id) == title_text)
                return id;
        }
        return std::nullopt;
    }
};

std::vector<std::string> split_tags(std::string_view value) {
    std::vector<std::string> tags;
    std::size_t at = 0;
    while (at < value.size()) {
        const auto colon = std::min(value.find(':', at), value.size());
        const auto tag = trim(value.substr(at, colon - at));
        if (!tag.empty())
            tags.emplace_back(tag);
        at = colon + 1;
    }
    return tags;
}

/**
 * @brief The anchor of each heading: its :ID:, else the slug of its path,
 * numbered from 2 when an earlier heading took it.
 */
void assign_anchors(std::vector<types::heading_row>& headings) {
    std::vector<std::string> taken;
    for (auto& h : headings) {
        if (h.org_id) {
            h.anchor = *h.org_id;
            taken.push_back(h.anchor);
        }
    }
    for (auto& h : headings) {
        if (h.org_id)
            continue;
        std::string joined;
        for (const auto& part : h.path) {
            if (!joined.empty())
                joined += ' ';
            joined += part;
        }
        const auto base = slug(joined);
        auto anchor = base;
        for (int n = 2; std::ranges::contains(taken, anchor); ++n)
            anchor = std::format("{}-{}", base, n);
        taken.push_back(anchor);
        h.anchor = std::move(anchor);
    }
}

/**
 * @brief The cells of a table row's line, trimmed, without their bars.
 */
std::vector<std::string_view> cells_of(std::string_view line) {
    std::vector<std::string_view> cells;
    line = trim(line);
    if (!line.starts_with('|'))
        return cells;
    line.remove_prefix(1);
    while (!line.empty()) {
        const auto bar = line.find('|');
        cells.push_back(trim(line.substr(0, bar)));
        if (bar == std::string_view::npos)
            break;
        line.remove_prefix(bar + 1);
    }
    if (!cells.empty() && cells.back().empty())
        cells.pop_back();
    return cells;
}

bool is_result_heading(std::string_view title) {
    return title == "Result" || title == "Results";
}

/**
 * @brief The test definition of a test_scenario page: Scenario Info's
 * links and target, the steps under * Steps, and the definition hash.
 */
types::scenario_row scenario_of(const reader& r,
    const types::document_row& d) {
    const auto& t = r.t;
    types::scenario_row s;
    s.org_id = d.org_id;
    s.title = d.title;
    std::vector<ot::source_range> left_out;

    if (const auto info = r.top_heading("Scenario Info")) {
        const auto e = t.headings.elements[*info];
        const auto end = t.elements.subtree_ends[e];
        for (auto c = e.next(); c < end; c = c.next()) {
            if (t.elements.kinds[c] != ot::element_kind::table_row)
                continue;
            const auto range = t.elements.ranges[c];
            const auto cells = cells_of(r.text_of(range));
            if (cells.size() < 2)
                continue;
            const auto first_id_link = [&]() -> types::nullable {
                for (std::uint32_t l = 0; l < t.links.size(); ++l) {
                    const ot::link_id id{l};
                    const auto at = r.link_range(id).begin;
                    if (at >= range.begin && at < range.end &&
                        name_text(t, t.links.types[id]) == "id")
                        return std::string(r.text_of(t.links.paths[id]));
                }
                return std::nullopt;
            };
            if (cells[0] == "Verifies task")
                s.task_id = first_id_link();
            else if (cells[0] == "Parent story")
                s.story_id = first_id_link();
            else if (cells[0].starts_with("Target") && !cells[1].empty())
                s.target = std::string(cells[1]);
            else if (cells[0] == "State")
                left_out.push_back(range);
        }
    }
    if (!s.target) {
        const auto k = compass::org::ops::find_keyword(t, "target_dialog");
        if (!k.is_none()) {
            const auto v = r.text_of(trimmed(r.source, t.keywords.values[k]));
            if (!v.empty())
                s.target = std::string(v);
        }
    }

    const auto steps = r.top_heading("Steps");
    for (std::uint32_t h = 0; h < t.headings.size(); ++h) {
        const ot::heading_id id{h};
        const auto e = t.headings.elements[id];
        if (is_result_heading(r.title(id)))
            left_out.push_back(t.elements.ranges[e]);
        if (!steps || t.elements.parents[e] != t.headings.elements[*steps])
            continue;
        std::vector<ot::source_range> results;
        for (auto sub = h + 1; sub < t.headings.size(); ++sub) {
            const ot::heading_id sid{sub};
            const auto se = t.headings.elements[sid];
            if (t.elements.ranges[se].begin >= t.elements.ranges[e].end)
                break;
            if (t.elements.parents[se] == e && is_result_heading(r.title(sid)))
                results.push_back(t.elements.ranges[se]);
        }
        const auto body = t.elements.contents[e];
        std::string text;
        auto at = body.begin;
        for (const auto& res : results) {
            text += r.source.substr(at, res.begin - at);
            at = res.end;
        }
        if (at < body.end)
            text += r.source.substr(at, body.end - at);
        s.steps.push_back({r.title(id), std::string(trim(text))});
    }

    std::ranges::sort(left_out, {}, &ot::source_range::begin);
    std::string kept;
    std::uint32_t at = 0;
    for (const auto& range : left_out) {
        if (range.begin > at)
            kept += r.source.substr(at, range.begin - at);
        at = std::max(at, range.end);
    }
    if (at < r.source.size())
        kept += r.source.substr(at);
    s.definition = store::ops::blob_id_of(kept);
    return s;
}

}

std::string slug(std::string_view text) {
    std::string out;
    bool hyphen = false;
    for (const char c : text) {
        const auto u = static_cast<unsigned char>(c);
        if ((u >= 'a' && u <= 'z') || (u >= '0' && u <= '9')) {
            if (hyphen && !out.empty())
                out += '-';
            hyphen = false;
            out += c;
        } else if (u >= 'A' && u <= 'Z') {
            if (hyphen && !out.empty())
                out += '-';
            hyphen = false;
            out += static_cast<char>(u - 'A' + 'a');
        } else {
            hyphen = true;
        }
    }
    return out.empty() ? std::string("section") : out;
}

types::corpus_document extract(const store::types::blob_id& hash,
    std::string_view source, const ot::tables_view& t) {
    const reader r(source, t);
    types::corpus_document out;
    out.hash = hash;
    auto& d = out.document;
    d.source = source;

    json_object keywords;
    for (std::uint32_t k = 0; k < t.keywords.size(); ++k) {
        const ot::keyword_id id{k};
        if (!r.before_first_heading(t.keywords.elements[id]))
            continue;
        const auto name = name_text(t, t.keywords.names[id]);
        const auto value = r.text_of(trimmed(source, t.keywords.values[id]));
        keywords.add(name, value);
        const auto set = [&](types::nullable& column) {
            if (!column && !value.empty())
                column = std::string(value);
        };
        if (name == "title")
            set(d.title);
        else if (name == "description")
            set(d.description);
        else if (name == "type")
            set(d.type);
        else if (name == "level")
            set(d.level);
        else if (name == "audience")
            set(d.audience);
        else if (name == "created")
            set(d.created);
        else if (name == "updated")
            set(d.updated);
        else if (name == "filetags")
            std::ranges::move(split_tags(value), std::back_inserter(d.tags));
    }
    d.keywords = keywords.text(true);
    std::optional<std::string_view> id;
    d.properties = r.properties_json(ot::heading_id::none(), &id);
    if (id && !id->empty())
        d.org_id = std::string(*id);

    std::vector<std::pair<std::uint16_t, std::string>> stack;
    for (std::uint32_t h = 0; h < t.headings.size(); ++h) {
        const ot::heading_id hid{h};
        types::heading_row row;
        row.level = t.headings.levels[hid];
        row.title = r.title(hid);
        while (!stack.empty() && stack.back().first >= row.level)
            stack.pop_back();
        for (const auto& [level, title] : stack)
            row.path.push_back(title);
        row.path.push_back(row.title);
        stack.emplace_back(t.headings.levels[hid], row.title);
        if (const auto todo = t.headings.todos[hid]; !todo.is_none())
            row.todo = std::string(name_text(t, todo));
        if (const auto p = t.headings.priorities[hid]; p != 0)
            row.priority = std::string(1, static_cast<char>(p));
        const auto tag_end = h + 1 < t.headings.size()
            ? t.headings.tag_firsts[hid.next()].value
            : static_cast<std::uint32_t>(t.heading_tags.size());
        for (auto g = t.headings.tag_firsts[hid].value; g < tag_end; ++g)
            row.tags.emplace_back(
                name_text(t, t.heading_tags.names[ot::tag_id{g}]));
        std::optional<std::string_view> heading_id;
        row.properties = r.properties_json(hid, &heading_id);
        if (heading_id && !heading_id->empty())
            row.org_id = std::string(*heading_id);
        out.headings.push_back(std::move(row));
    }
    assign_anchors(out.headings);

    if (const auto preamble = r.section_under(ot::element_id{0})) {
        auto text = r.section_text(*preamble);
        if (!text.empty())
            out.sections.push_back({std::nullopt, std::move(text)});
    }
    for (std::uint32_t h = 0; h < t.headings.size(); ++h) {
        const auto s = r.section_under(t.headings.elements[ot::heading_id{h}]);
        out.sections.push_back({h, s ? r.section_text(*s) : std::string()});
    }

    out.links.reserve(t.links.size());
    for (std::uint32_t l = 0; l < t.links.size(); ++l) {
        const ot::link_id lid{l};
        types::link_row row;
        row.heading = r.heading_at(r.link_range(lid).begin);
        row.type = name_text(t, t.links.types[lid]);
        row.path = r.text_of(t.links.paths[lid]);
        if (row.type == "id")
            row.target_id = row.path;
        row.text = r.link_text(lid);
        out.links.push_back(std::move(row));
    }

    if (d.type == "test_scenario") {
        out.scenario = scenario_of(r, d);
        const auto k = compass::org::ops::find_keyword(t, "environment");
        if (!k.is_none()) {
            const auto v = r.text_of(trimmed(source, t.keywords.values[k]));
            if (!v.empty())
                out.scenario->environment = std::string(v);
        }
    }
    return out;
}

}

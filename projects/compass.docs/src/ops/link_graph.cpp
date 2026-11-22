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
#include "compass.docs/ops/link_graph.hpp"

#include <algorithm>
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.docs/ops/org_text.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::ops {

namespace {

namespace ct = compass::codec::types;
namespace ot = compass::org::types;
namespace oo = compass::org::ops;

constexpr std::uint32_t described_flag = 1;
constexpr std::uint32_t ordering_flag = 2;
constexpr std::uint32_t aside_flag = 4;

/**
 * @brief True when a comment line marks its section as set aside: "#",
 * then "aside" as its first word.
 */
bool is_aside(std::string_view line) {
    auto rest = compass::cpp::ops::strip(line);
    rest.remove_prefix(1);
    rest = compass::cpp::ops::strip(rest);
    return rest.starts_with("aside") &&
        (rest.size() == 5 || !compass::cpp::ops::is_word(rest[5]));
}

/**
 * @brief True for a target shaped like an ID: 36 hexadecimal digits and
 * hyphens.
 */
bool is_id(std::string_view target) {
    return target.size() == 36 && std::ranges::all_of(target, [](char c) {
        return c == '-' || (c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

}

std::string upper(std::string_view s) {
    std::string out(s);
    for (auto& c : out) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    }
    return out;
}

std::string fact_of(const ct::document& d) {
    static const compass::platform::text::regex source_marker(
        R"(^#\s*BEGIN generated\s+\S+\s+\(([^)]+)\)\s*$)");
    const auto& t = d.tables;
    const auto source = d.source;
    std::string out;
    ct::fact_writer fact(out);
    fact.number(compass::platform::text::is_utf8(source) ? 1 : 0);
    fact.text(first_word(keyword_value(d, "type")));
    fact.text(keyword_value(d, "title"));
    fact.number(compass::org::ops::has_tag(keyword_value(d, "filetags"),
        "structure_note") ? 1 : 0);
    fact.number(compass::org::ops::is_evidence(source, t) ? 1 : 0);

    const auto& h = t.headings;
    std::vector<std::uint32_t> starts;
    std::vector<std::string_view> sections;
    std::vector<std::uint32_t> section_starts;
    std::uint32_t see_also = 0;
    auto cut = static_cast<std::uint32_t>(source.size());
    for (std::uint32_t i = 0; i < h.size(); ++i) {
        const ot::heading_id id{i};
        const auto begin = t.elements.ranges[h.elements[id]].begin;
        starts.push_back(begin);
        if (h.levels[id] != 1)
            continue;
        const auto title = oo::trimmed(source, h.titles[id]).of(source);
        sections.push_back(title);
        section_starts.push_back(begin);
        if (title == "See also" && see_also == 0)
            see_also = begin;
        if (title == "See also" || title == "What this cluster does not cover")
            cut = std::min(cut, begin);
    }
    fact.number(see_also);

    const auto& p = t.properties;
    std::uint32_t ids = 0;
    for (std::uint32_t i = 0; i < p.size(); ++i)
        ids += oo::name_text(t, p.names[ot::property_id{i}]) == "ID" ? 1 : 0;
    fact.number(ids);
    for (std::uint32_t i = 0; i < p.size(); ++i) {
        const ot::property_id id{i};
        if (oo::name_text(t, p.names[id]) == "ID")
            fact.text(upper(oo::trimmed(source, p.values[id]).of(source)));
    }
    fact.number(static_cast<std::uint32_t>(sections.size()));
    for (const auto s : sections)
        fact.text(s);

    // Slot 0 is the text before the first heading, slot h + 1 heading h's
    // own section.
    const auto slot = [&](std::uint32_t offset) {
        return static_cast<std::size_t>(
            std::ranges::upper_bound(starts, offset) - starts.begin());
    };
    std::vector<std::uint8_t> aside(starts.size() + 1, 0);
    std::vector<marker> markers;
    for (const auto& c : comments(d)) {
        for (const auto line : c.lines) {
            if (is_aside(line))
                aside[slot(c.begin)] = 1;
            if (const auto m = source_marker.match(line))
                markers.push_back({line_start(source, static_cast<
                    std::uint32_t>(line.data() - source.data())),
                    m->group(1)});
        }
    }

    auto links = bracket_links(d, "id");
    std::erase_if(links, [](const auto& l) { return !is_id(l.target); });
    fact.number(static_cast<std::uint32_t>(links.size()));
    for (const auto& l : links) {
        const auto s = static_cast<std::size_t>(std::ranges::upper_bound(
            section_starts, l.begin) - section_starts.begin());
        std::uint32_t flags = l.label.empty() ? 0 : described_flag;
        flags |= l.begin < cut ? ordering_flag : 0;
        flags |= aside[slot(l.begin)] != 0 ? aside_flag : 0;
        fact.number(line_start(source, l.begin));
        fact.text(l.target);
        fact.text(l.label);
        fact.number(flags);
        fact.number(s == 0 ? no_section : static_cast<std::uint32_t>(s - 1));
    }
    fact.number(static_cast<std::uint32_t>(markers.size()));
    for (const auto& m : markers) {
        fact.number(m.line);
        fact.text(m.script);
    }
    return out;
}

page page_of(std::string_view path, std::string_view fact) {
    ct::fact_reader r(fact);
    page out;
    out.path = path;
    out.utf8 = r.number() != 0;
    out.type = r.text();
    out.title = r.text();
    out.structure_note = r.number() != 0;
    out.evidence = r.number() != 0;
    out.see_also = r.number();
    const auto ids = r.number();
    out.ids.reserve(ids);
    for (std::uint32_t i = 0; i < ids; ++i)
        out.ids.push_back(r.text());
    const auto sections = r.number();
    out.sections.reserve(sections);
    for (std::uint32_t i = 0; i < sections; ++i)
        out.sections.push_back(r.text());
    const auto links = r.number();
    out.links.reserve(links);
    for (std::uint32_t i = 0; i < links; ++i) {
        auto& l = out.links.emplace_back();
        l.line = r.number();
        l.target = r.text();
        l.label = r.text();
        const auto flags = r.number();
        l.described = (flags & described_flag) != 0;
        l.ordering = (flags & ordering_flag) != 0;
        l.aside = (flags & aside_flag) != 0;
        l.section = r.number();
    }
    const auto markers = r.number();
    out.markers.reserve(markers);
    for (std::uint32_t i = 0; i < markers; ++i) {
        auto& m = out.markers.emplace_back();
        m.line = r.number();
        m.script = r.text();
    }
    return out;
}

}

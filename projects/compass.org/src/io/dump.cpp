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
#include "compass.org/io/dump.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace compass::org::io {

namespace {

using namespace types;

std::string_view name_of(element_kind k) noexcept {
    switch (k) {
    case element_kind::document: return "org-data";
    case element_kind::section: return "section";
    case element_kind::heading: return "headline";
    case element_kind::inlinetask: return "inlinetask";
    case element_kind::plain_list: return "plain-list";
    case element_kind::item: return "item";
    case element_kind::table: return "table";
    case element_kind::table_el: return "table";
    case element_kind::table_row: return "table-row";
    case element_kind::table_rule: return "table-row";
    case element_kind::drawer: return "drawer";
    case element_kind::property_drawer: return "property-drawer";
    case element_kind::node_property: return "node-property";
    case element_kind::footnote_definition: return "footnote-definition";
    case element_kind::quote_block: return "quote-block";
    case element_kind::center_block: return "center-block";
    case element_kind::special_block: return "special-block";
    case element_kind::dynamic_block: return "dynamic-block";
    case element_kind::verse_block: return "verse-block";
    case element_kind::src_block: return "src-block";
    case element_kind::example_block: return "example-block";
    case element_kind::export_block: return "export-block";
    case element_kind::comment_block: return "comment-block";
    case element_kind::paragraph: return "paragraph";
    case element_kind::keyword: return "keyword";
    case element_kind::babel_call: return "babel-call";
    case element_kind::planning: return "planning";
    case element_kind::clock: return "clock";
    case element_kind::diary_sexp: return "diary-sexp";
    case element_kind::comment: return "comment";
    case element_kind::fixed_width: return "fixed-width";
    case element_kind::horizontal_rule: return "horizontal-rule";
    case element_kind::latex_environment: return "latex-environment";
    }
    return "unknown";
}

std::string_view name_of(object_kind k) noexcept {
    switch (k) {
    case object_kind::bold: return "bold";
    case object_kind::italic: return "italic";
    case object_kind::underline: return "underline";
    case object_kind::strike_through: return "strike-through";
    case object_kind::code: return "code";
    case object_kind::verbatim: return "verbatim";
    case object_kind::link: return "link";
    case object_kind::timestamp: return "timestamp";
    case object_kind::footnote_reference: return "footnote-reference";
    case object_kind::inline_src_block: return "inline-src-block";
    case object_kind::inline_babel_call: return "inline-babel-call";
    case object_kind::macro: return "macro";
    case object_kind::export_snippet: return "export-snippet";
    case object_kind::latex_fragment: return "latex-fragment";
    case object_kind::entity: return "entity";
    case object_kind::statistics_cookie: return "statistics-cookie";
    case object_kind::radio_target: return "radio-target";
    case object_kind::target: return "target";
    case object_kind::line_break: return "line-break";
    case object_kind::subscript: return "subscript";
    case object_kind::superscript: return "superscript";
    case object_kind::table_cell: return "table-cell";
    case object_kind::citation: return "citation";
    case object_kind::citation_reference: return "citation-reference";
    }
    return "unknown";
}

/**
 * @brief Whether org-element keeps every occurrence of an affiliated
 * keyword on one element, not only the last.
 */
bool repeats(std::string_view name) noexcept {
    return name == "caption" || name == "header" || name.starts_with("attr_");
}

class writer final {
public:
    writer(std::ostream& os, std::string_view source, const tables& t)
        : os_(os), source_(source), t_(t) {}

    void run() {
        std::vector<std::uint32_t> ends;
        for (std::uint32_t i = 0; i < t_.elements.size(); ++i) {
            while (!ends.empty() && ends.back() <= i)
                ends.pop_back();
            const element_id e{i};
            const auto depth = static_cast<std::uint32_t>(ends.size());
            element(e, depth);
            affiliated(e, depth + 1);
            const auto last = i + 1 < t_.elements.size()
                ? t_.elements.object_firsts.values[i + 1].value
                : static_cast<std::uint32_t>(t_.objects.size());
            objects(t_.elements.object_firsts[e].value, last, depth + 1);
            ends.push_back(t_.elements.subtree_ends[e].value);
        }
    }

private:
    std::string_view name(name_id id) const {
        if (id.is_none())
            return "-";
        const auto first = t_.names.firsts[id];
        const auto next = id.value + 1 < t_.names.size()
            ? t_.names.firsts.values[id.value + 1]
            : static_cast<std::uint32_t>(t_.name_bytes.size());
        return {t_.name_bytes.data() + first, next - first};
    }

    void indent(std::uint32_t depth) {
        for (std::uint32_t i = 0; i < depth; ++i)
            os_ << "  ";
    }

    void text(source_range r) { text(r.of(source_)); }

    void text(std::string_view value) {
        for (const char ch : value) {
            switch (ch) {
            case '\\': os_ << "\\\\"; break;
            case '\n': os_ << "\\n"; break;
            case '\r': os_ << "\\r"; break;
            case '\t': os_ << "\\t"; break;
            default: os_ << ch; break;
            }
        }
    }

    void positions(source_range range, source_range contents,
        std::uint8_t post_blank) {
        os_ << ' ' << range.begin << ' ' << range.end << ' ';
        if (contents.empty())
            os_ << "- -";
        else
            os_ << contents.begin << ' ' << contents.end;
        os_ << ' ' << static_cast<unsigned>(post_blank);
    }

    void element(element_id e, std::uint32_t depth) {
        const auto kind = t_.elements.kinds[e];
        indent(depth);
        os_ << name_of(kind);
        positions(t_.elements.ranges[e], t_.elements.contents[e],
            t_.elements.post_blanks[e]);
        switch (kind) {
        case element_kind::heading:
        case element_kind::inlinetask:
            heading();
            break;
        case element_kind::keyword:
        case element_kind::babel_call: {
            const keyword_id k{keyword_++};
            os_ << " key=" << name(t_.keywords.names[k]) << " value=";
            text(t_.keywords.values[k]);
            break;
        }
        case element_kind::node_property: {
            const property_id p{property_++};
            os_ << " key=" << name(t_.properties.names[p]) << " value=";
            text(t_.properties.values[p]);
            break;
        }
        case element_kind::src_block:
        case element_kind::special_block:
        case element_kind::dynamic_block: {
            const block_id b{block_++};
            os_ << " name=" << name(t_.blocks.names[b]) << " parameters=";
            text(t_.blocks.parameters[b]);
            break;
        }
        case element_kind::example_block:
            os_ << " parameters=";
            text(t_.blocks.parameters[block_id{block_++}]);
            break;
        case element_kind::export_block:
            os_ << " name=" << name(t_.blocks.names[block_id{block_++}]);
            break;
        case element_kind::quote_block:
        case element_kind::center_block:
        case element_kind::verse_block:
        case element_kind::comment_block:
            ++block_;
            break;
        case element_kind::plain_list: {
            const auto type = t_.lists.types[list_id{list_++}];
            os_ << " type="
                << (type == list_type::ordered ? "ordered"
                    : type == list_type::descriptive ? "descriptive"
                    : "unordered");
            break;
        }
        case element_kind::item: {
            const auto box = t_.items.checkboxes[item_id{item_++}];
            os_ << " checkbox="
                << (box == checkbox::on ? "on" : box == checkbox::off ? "off"
                    : box == checkbox::partial ? "trans" : "-");
            break;
        }
        case element_kind::table: os_ << " type=org"; break;
        case element_kind::table_el: os_ << " type=table.el"; break;
        case element_kind::table_row: os_ << " type=standard"; break;
        case element_kind::table_rule: os_ << " type=rule"; break;
        default:
            break;
        }
        os_ << '\n';
    }

    void heading() {
        const heading_id h{heading_++};
        const auto& t = t_.headings;
        const auto type = t.todo_types[h];
        os_ << " level=" << t.levels[h] << " todo=" << name(t.todos[h])
            << " todo-type="
            << (type == todo_type::todo ? "todo"
                : type == todo_type::done ? "done" : "-")
            << " priority=";
        if (t.priorities[h] == 0)
            os_ << '-';
        else
            os_ << static_cast<unsigned>(t.priorities[h]);
        os_ << " commented="
            << (t.comments[h] == comment_state::commented ? 1 : 0)
            << " tags=";
        const auto first = t.tag_firsts[h].value;
        const auto last = h.value + 1 < t.size()
            ? t.tag_firsts.values[h.value + 1].value
            : static_cast<std::uint32_t>(t_.heading_tags.size());
        if (first == last)
            os_ << '-';
        for (auto i = first; i < last; ++i) {
            if (i > first)
                os_ << ':';
            os_ << name(t_.heading_tags.names[tag_id{i}]);
        }
        os_ << " title=";
        text(t.titles[h]);
    }

    void affiliated(element_id owner, std::uint32_t depth) {
        const auto& a = t_.affiliated;
        const auto first = affiliated_;
        while (affiliated_ < a.size() &&
            a.owners[affiliated_id{affiliated_}] == owner)
            ++affiliated_;
        if (first == affiliated_)
            return;
        std::vector<std::uint32_t> rows;
        for (auto i = first; i < affiliated_; ++i) {
            const auto key = name(a.names[affiliated_id{i}]);
            bool superseded = false;
            for (auto j = i + 1; j < affiliated_ && !repeats(key); ++j)
                superseded = superseded ||
                    a.names[affiliated_id{j}] == a.names[affiliated_id{i}];
            if (!superseded)
                rows.push_back(i);
        }
        std::ranges::stable_sort(rows, [&](auto x, auto y) {
            return name(a.names[affiliated_id{x}]) <
                name(a.names[affiliated_id{y}]);
        });
        for (const auto i : rows) {
            const affiliated_id id{i};
            const auto key = name(a.names[id]);
            indent(depth);
            os_ << "affiliated key=" << key;
            if (key != "caption") {
                os_ << " value=";
                text(a.values[id]);
                os_ << '\n';
                continue;
            }
            os_ << '\n';
            const auto last = i + 1 < a.size()
                ? a.object_firsts.values[i + 1].value
                : t_.elements.object_firsts.values[0].value;
            objects(a.object_firsts[id].value, last, depth + 1);
        }
    }

    void objects(std::uint32_t first, std::uint32_t last,
        std::uint32_t depth) {
        std::vector<std::uint32_t> ends;
        for (auto i = first; i < last; ++i) {
            while (!ends.empty() && ends.back() <= i)
                ends.pop_back();
            const object_id o{i};
            const auto kind = t_.objects.kinds[o];
            indent(depth + static_cast<std::uint32_t>(ends.size()));
            os_ << name_of(kind);
            positions(t_.objects.ranges[o], t_.objects.contents[o],
                t_.objects.post_blanks[o]);
            if (kind == object_kind::link)
                link(o);
            os_ << '\n';
            ends.push_back(t_.objects.subtree_ends[o].value);
        }
    }

    /**
     * @brief Links are in object order, but the objects of captions come
     * first in the table and print later, so the row is searched for.
     */
    void link(object_id o) {
        const auto& l = t_.links;
        const auto it = std::ranges::lower_bound(l.objects.values, o);
        const link_id id{
            static_cast<std::uint32_t>(it - l.objects.values.begin())};
        const auto format = l.formats[id];
        os_ << " type=" << name(l.types[id]) << " format="
            << (format == link_format::plain ? "plain"
                : format == link_format::angle ? "angle" : "bracket")
            << " path=";
        path(l.paths[id], format);
    }

    /**
     * @brief A path prints as org-element reads it. A line break and the
     * blanks around it are one space in a bracket link and nothing in an
     * angle link. In a bracket link, backslashes before a bracket or at the
     * end escape, so half of them print.
     */
    void path(source_range r, link_format format) {
        const auto raw = r.of(source_);
        const bool bracket = format == link_format::bracket;
        std::string out;
        for (std::size_t i = 0; i < raw.size();) {
            const char ch = raw[i];
            if (ch == '\n') {
                while (!out.empty() && (out.back() == ' ' ||
                    out.back() == '\t' || out.back() == '\r'))
                    out.pop_back();
                ++i;
                while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\t'))
                    ++i;
                if (bracket)
                    out += ' ';
            } else if (ch == '\\' && bracket) {
                auto j = i;
                while (j < raw.size() && raw[j] == '\\')
                    ++j;
                const bool escapes =
                    j == raw.size() || raw[j] == '[' || raw[j] == ']';
                out.append(escapes ? (j - i) / 2 : j - i, '\\');
                i = j;
            } else {
                out += ch;
                ++i;
            }
        }
        text(out);
    }

    std::ostream& os_;
    std::string_view source_;
    const tables& t_;
    std::uint32_t heading_ = 0;
    std::uint32_t keyword_ = 0;
    std::uint32_t property_ = 0;
    std::uint32_t block_ = 0;
    std::uint32_t list_ = 0;
    std::uint32_t item_ = 0;
    std::uint32_t affiliated_ = 0;
};

}

void dump(std::ostream& os, std::string_view source, const tables& tables) {
    writer(os, source, tables).run();
}

}

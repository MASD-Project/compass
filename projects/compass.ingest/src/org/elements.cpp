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
#include <cstdint>
#include <string_view>
#include <vector>
#include "context.hpp"

namespace compass::ingest::org::detail {

namespace {

using types::element_id;
using types::element_kind;

/**
 * @brief What org-element expects next, which decides how a line is read.
 */
enum class mode : std::uint8_t {
    none,
    planning,
    property_drawer,
    top_comment,
    item,
    table_row,
    node_property
};

/**
 * @brief One line that starts a heading, and where its subtree ends.
 */
struct heading_line {
    pos begin;
    pos end;
    std::uint32_t level;
};

/**
 * @brief One item of a list structure: where it starts and ends, the column
 * of its bullet and whether it has a description tag.
 */
struct list_entry {
    pos begin;
    pos end;
    std::uint32_t indent;
    bool tagged;
};

/**
 * @brief The entries of one list structure: a run of the entry stack.
 */
struct structure {
    std::uint32_t first = 0;
    std::uint32_t count = 0;
};

/**
 * @brief The fields of the line that opens an item.
 */
struct item_fields {
    bool ordered = false;
    bool bullet_tags = false;
    types::checkbox box = types::checkbox::none;
    bool tagged = false;
    pos tag_begin = 0;
    pos tag_end = 0;
    pos after = 0;
};

/**
 * @brief An affiliated keyword line.
 */
struct affiliated_fields {
    std::string_view name;
    types::source_range option;
    types::source_range value;
};

/**
 * @brief The element a parser appended.
 */
struct parsed {
    element_id id;
    element_kind kind = element_kind::paragraph;
    pos end = 0;
    pos cbeg = 0;
    pos cend = 0;
    structure list;
    bool owns_list = false;
};

constexpr std::uint32_t tab_width = 8;

class element_parser final {
public:
    explicit element_parser(context& c) noexcept
        : c_(c), src_(c.src), n_(c.src.size()) {}

    void run() {
        const pos cbeg = src_.bol(src_.skip_ws(0, n_));
        const auto doc = c_.add_element(element_kind::document, 0, n_, cbeg,
            n_, 0, element_id::none());
        scan_headings();
        std::size_t next_heading = 0;
        outline(cbeg, n_, true, doc, next_heading);
        close(doc);
    }

private:
    void close(element_id id) {
        c_.t.elements.subtree_ends[id] =
            element_id{static_cast<std::uint32_t>(c_.t.elements.size())};
    }

    void scan_headings() {
        for (pos p = 0; p < n_; p = src_.next_line(p)) {
            if (src_[p] != '*')
                continue;
            pos q = p;
            while (q < n_ && src_[q] == '*')
                ++q;
            if (q < n_ && src_[q] == ' ')
                headings_.push_back({p, n_, q - p});
        }
        std::vector<std::uint32_t>& open = stack_;
        open.clear();
        for (std::uint32_t i = 0; i < headings_.size(); ++i) {
            while (!open.empty() &&
                headings_[open.back()].level >= headings_[i].level) {
                headings_[open.back()].end = headings_[i].begin;
                open.pop_back();
            }
            open.push_back(i);
        }
    }

    /**
     * @brief The sections and headings of [beg, end), which is the contents
     * of the document or of a heading.
     */
    void outline(pos beg, pos end, bool first, element_id parent,
        std::size_t& next_heading) {
        pos p = beg;
        while (p < end) {
            if (next_heading < headings_.size() &&
                headings_[next_heading].begin == p) {
                const auto h = headings_[next_heading++];
                const pos q = src_.skip_ws(src_.next_line(h.begin), h.end);
                const bool filled = q != h.end;
                const pos cbeg = filled ? src_.bol(q) : 0;
                const auto id = c_.add_element(element_kind::heading,
                    h.begin, h.end, cbeg, filled ? h.end : 0,
                    filled ? 0 : src_.count_lines(h.begin, h.end) - 1,
                    parent);
                add_heading(id, h.level);
                if (filled)
                    outline(cbeg, h.end, false, id, next_heading);
                close(id);
                p = h.end;
                continue;
            }
            const pos stop = next_heading < headings_.size()
                ? std::min(end, headings_[next_heading].begin) : end;
            const auto id = c_.add_element(element_kind::section, p, stop, p,
                stop, 0, parent);
            elements(p, stop, first ? mode::top_comment : mode::planning, id,
                {});
            close(id);
            p = stop;
        }
    }

    void add_heading(element_id id, std::uint32_t level) {
        auto& h = c_.t.headings;
        h.elements.values.push_back(id);
        h.levels.values.push_back(static_cast<std::uint16_t>(
            std::min<std::uint32_t>(level, 0xFFFF)));
        h.todos.values.push_back(types::name_id::none());
        h.todo_types.values.push_back(types::todo_type::none);
        h.priorities.values.push_back(0);
        h.comments.values.push_back(types::comment_state::live);
        h.titles.values.push_back({});
        h.tag_firsts.values.push_back(types::tag_id{0});
        h.property_firsts.values.push_back(types::property_id{
            static_cast<std::uint32_t>(c_.t.properties.size())});
    }

    static mode next_mode(mode m, element_kind k) noexcept {
        switch (m) {
        case mode::item:
        case mode::table_row:
        case mode::node_property:
            return m;
        case mode::planning:
            return k == element_kind::planning ? mode::property_drawer
                                               : mode::none;
        case mode::top_comment:
            return k == element_kind::comment ? mode::property_drawer
                                              : mode::none;
        default:
            return mode::none;
        }
    }

    void elements(pos beg, pos end, mode m, element_id parent, structure st) {
        pos p = beg;
        while (p < end) {
            const auto r = element(p, end, m, parent, st);
            switch (r.kind) {
            case element_kind::plain_list:
                elements(r.cbeg, r.cend, mode::item, r.id, r.list);
                if (r.owns_list)
                    entries_.resize(r.list.first);
                break;
            case element_kind::item:
                elements(r.cbeg, r.cend, mode::none, r.id, st);
                break;
            case element_kind::table:
                elements(r.cbeg, r.cend, mode::table_row, r.id, {});
                break;
            case element_kind::property_drawer:
                elements(r.cbeg, r.cend, mode::node_property, r.id, {});
                break;
            case element_kind::drawer:
            case element_kind::footnote_definition:
            case element_kind::quote_block:
            case element_kind::center_block:
            case element_kind::special_block:
            case element_kind::dynamic_block:
                elements(r.cbeg, r.cend, mode::none, r.id, {});
                break;
            default:
                break;
            }
            close(r.id);
            p = std::max(r.end, p + 1);
            m = next_mode(m, r.kind);
        }
    }

    parsed element(pos p, pos limit, mode m, element_id parent,
        structure st) {
        switch (m) {
        case mode::item: return item(p, limit, parent, st);
        case mode::table_row: return table_row(p, parent);
        case mode::node_property: return node_property(p, parent);
        default: break;
        }
        if (!src_.at_bol(p))
            return paragraph(p, p, limit, parent);
        if (comment_line(p))
            return comment(p, limit, parent);
        const pos previous = p == 0 ? 0 : src_.bol(p - 1);
        const bool after_heading =
            m == mode::planning && src_[previous] == '*';
        if (after_heading && planning_line(p))
            return line_element(element_kind::planning, p, limit, parent);
        const bool drawer_allowed = after_heading ||
            ((m == mode::property_drawer || m == mode::top_comment) &&
                (!src_.blank_line(previous) ||
                    src_.skip_ws_back(previous, 0) == 0));
        if (drawer_allowed && property_drawer_at(p))
            return property_drawer(p, limit, parent);
        if (clock_line(p))
            return line_element(element_kind::clock, p, limit, parent);

        const pos after = affiliated_end(p, limit);
        const auto r = dispatch(after, p, limit, parent, st);
        if (after != p)
            add_affiliated(p, after, r.id);
        return r;
    }

    /**
     * @brief Where the affiliated keywords that start at p stop, or p when
     * they attach to nothing.
     */
    pos affiliated_end(pos p, pos limit) const {
        pos q = p;
        affiliated_fields f;
        while (q < limit && affiliated_line(q, f))
            q = src_.next_line(q);
        if (q == p)
            return p;
        if (q >= limit || src_.blank_line(q) || comment_line(q) ||
            clock_line(q) || heading_line_at(q))
            return p;
        return q;
    }

    void add_affiliated(pos p, pos after, element_id owner) {
        auto& a = c_.t.affiliated;
        affiliated_fields f;
        for (pos q = p; q < after; q = src_.next_line(q)) {
            affiliated_line(q, f);
            a.owners.values.push_back(owner);
            a.names.values.push_back(c_.intern(f.name, name_case::lower));
            a.options.values.push_back(f.option);
            a.values.values.push_back(f.value);
            a.object_firsts.values.push_back(types::object_id{0});
        }
    }

    bool affiliated_line(pos p, affiliated_fields& f) const {
        const pos i = src_.skip_blank(p);
        if (!src_.match(i, "#+"))
            return false;
        pos j = i + 2;
        while (j < n_ && (is_ascii_alnum(src_[j]) || src_[j] == '_' ||
            src_[j] == '-'))
            ++j;
        const auto key = src_.view(i + 2, j);
        const pos e = src_.eol(p);
        f.option = {};
        pos colon = j;
        if (equals_ci(key, "caption") || equals_ci(key, "results")) {
            f.name = equals_ci(key, "caption") ? "caption" : "results";
            if (src_.at(j) == '[') {
                pos close = npos;
                for (pos k = e; k > j + 1; --k) {
                    if (src_[k - 1] == ']' && src_.at(k) == ':' && k < e) {
                        close = k - 1;
                        break;
                    }
                }
                if (close == npos)
                    return false;
                f.option = {j + 1, close};
                colon = close + 1;
            }
        } else if (key.size() > 5 && equals_ci(key.substr(0, 5), "attr_")) {
            f.name = key;
        } else {
            f.name = translated(key);
            if (f.name.empty())
                return false;
        }
        if (src_.at(colon) != ':' || colon >= e)
            return false;
        const pos vb = src_.skip_blank(colon + 1, e);
        f.value = {vb, src_.skip_blank_back(e, vb)};
        return true;
    }

    static bool equals_ci(std::string_view a, std::string_view lower)
        noexcept {
        if (a.size() != lower.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (to_lower(a[i]) != lower[i])
                return false;
        }
        return true;
    }

    /**
     * @brief The name org-element files an affiliated keyword under, or
     * empty when the key is not one.
     */
    static std::string_view translated(std::string_view key) noexcept {
        for (const auto k : {"data", "label", "resname", "source", "srcname",
            "tblname", "name"}) {
            if (equals_ci(key, k))
                return "name";
        }
        if (equals_ci(key, "header") || equals_ci(key, "headers"))
            return "header";
        if (equals_ci(key, "result"))
            return "results";
        if (equals_ci(key, "plot"))
            return "plot";
        return {};
    }

    parsed dispatch(pos p, pos begin, pos limit, element_id parent,
        structure st) {
        const pos i = src_.skip_blank(p);
        const char ch = src_.at(i);
        switch (ch) {
        case '\\':
            if (latex_begin(i) != npos)
                return latex_environment(p, begin, limit, parent);
            break;
        case ':':
            if (drawer_line(p))
                return drawer(p, begin, limit, parent);
            if (src_.at(i + 1) == ' ' || src_.at_eol(i + 1))
                return fixed_width(p, begin, limit, parent);
            break;
        case '#':
            if (src_.at(i + 1) != '+')
                break;
            if (src_.match_ci(i + 2, "begin:") &&
                src_.skip_blank(i + 8) < n_ &&
                src_.word_at(src_.skip_blank(i + 8)))
                return dynamic_block(p, begin, limit, parent);
            if (src_.match_ci(i + 2, "begin_") && !space_or_end(i + 8))
                return block(p, begin, limit, parent);
            if (src_.match_ci(i + 2, "call:"))
                return babel_call(p, begin, limit, parent);
            if (const pos colon = keyword_colon(i + 2);
                colon != npos && colon > i + 2)
                return keyword(p, begin, limit, parent);
            break;
        case '[':
            if (i == p && footnote_label_end(p) != npos)
                return footnote_definition(p, begin, limit, parent);
            break;
        case '-':
            if (rule_line(i))
                return leaf(element_kind::horizontal_rule, begin,
                    src_.next_line(p), limit, parent);
            break;
        case '%':
            if (i == p && src_.match(p, "%%("))
                return leaf(element_kind::diary_sexp, begin,
                    src_.next_line(p), limit, parent);
            break;
        case '|':
            return table(p, begin, limit, parent, true);
        case '+':
            if (table_el_at(p, limit))
                return table(p, begin, limit, parent, false);
            break;
        default:
            break;
        }
        if (item_at(p))
            return plain_list(p, begin, limit, parent, st);
        return paragraph(p, begin, limit, parent);
    }

    bool space_or_end(pos p) const noexcept {
        return p >= n_ || src_.space_at(p);
    }

    pos token_end(pos p) const noexcept {
        while (p < n_ && !src_.space_at(p))
            p += src_.width(p);
        return p;
    }

    bool heading_line_at(pos p) const noexcept {
        pos q = p;
        while (q < n_ && src_[q] == '*')
            ++q;
        return q > p && q < n_ && src_[q] == ' ';
    }

    bool comment_line(pos p) const noexcept {
        const pos i = src_.skip_blank(p);
        return src_.at(i) == '#' &&
            (src_.at(i + 1) == ' ' || src_.at_eol(i + 1));
    }

    bool planning_line(pos p) const noexcept {
        const pos i = src_.skip_blank(p);
        return src_.match_ci(i, "closed:") || src_.match_ci(i, "deadline:") ||
            src_.match_ci(i, "scheduled:");
    }

    bool drawer_line(pos p) const noexcept {
        const pos i = src_.skip_blank(p);
        if (src_.at(i) != ':')
            return false;
        pos j = i + 1;
        while (j < n_ && (src_[j] == '-' || src_[j] == '_' ||
            src_.word_at(j)))
            j += src_.width(j);
        return j > i + 1 && src_.at(j) == ':' &&
            src_.at_eol(src_.skip_blank(j + 1));
    }

    bool end_line(pos p) const noexcept {
        const pos i = src_.skip_blank(p);
        return src_.match_ci(i, ":end:") &&
            src_.at_eol(src_.skip_blank(i + 5));
    }

    /**
     * @brief The first line in [from, limit) that closes a drawer, or npos.
     */
    pos find_end_line(pos from, pos limit) const noexcept {
        for (pos l = from; l < limit; l = src_.next_line(l)) {
            if (end_line(l) && src_.eol(l) <= limit)
                return l;
        }
        return npos;
    }

    bool property_line(pos p) const noexcept {
        const pos i = src_.skip_blank(p);
        if (src_.at(i) != ':')
            return false;
        const pos e = token_end(i);
        return e - i >= 3 && src_[e - 1] == ':';
    }

    bool property_drawer_at(pos p) const noexcept {
        const pos i = src_.skip_blank(p);
        if (!src_.match_ci(i, ":properties:") ||
            !src_.at_eol(src_.skip_blank(i + 12)))
            return false;
        for (pos l = src_.next_line(p); l < n_ && l > p;
            l = src_.next_line(l)) {
            if (end_line(l))
                return true;
            if (!property_line(l) || src_.next_line(l) == l)
                return false;
            if (src_.eol(l) == n_)
                return false;
        }
        return false;
    }

    /**
     * @brief Where an inactive timestamp that starts at p ends, or npos.
     */
    pos inactive_timestamp(pos p) const noexcept {
        if (src_.at(p) != '[')
            return npos;
        for (const pos d : {1u, 2u, 3u, 4u, 6u, 7u, 9u, 10u}) {
            if (!is_digit(src_.at(p + d)))
                return npos;
        }
        if (src_.at(p + 5) != '-' || src_.at(p + 8) != '-')
            return npos;
        if (src_.at(p + 11) == ']')
            return p + 12;
        if (src_.at(p + 11) != ' ')
            return npos;
        const pos close = src_.find(p + 12, src_.eol(p), ']');
        return close == npos ? npos : close + 1;
    }

    pos duration(pos p) const noexcept {
        pos q = src_.skip_blank(p);
        if (q == p || !src_.match(q, "=>"))
            return npos;
        const pos r = src_.skip_blank(q + 2);
        if (r == q + 2)
            return npos;
        q = r;
        while (is_digit(src_.at(q)))
            ++q;
        if (q == r || src_.at(q) != ':' || !is_digit(src_.at(q + 1)) ||
            !is_digit(src_.at(q + 2)))
            return npos;
        return q + 3;
    }

    bool clock_line(pos p) const noexcept {
        const pos i = src_.skip_blank(p);
        if (!src_.match_ci(i, "clock:"))
            return false;
        pos q = i + 6;
        const pos stamp = src_.skip_blank(q);
        if (stamp > q && src_.at(stamp) == '[') {
            q = inactive_timestamp(stamp);
            if (q == npos)
                return false;
            if (src_.match(q, "--")) {
                q = inactive_timestamp(q + 2);
                if (q == npos)
                    return false;
                q = duration(q);
            }
        } else {
            q = duration(q);
        }
        return q != npos && src_.at_eol(src_.skip_blank(q));
    }

    /**
     * @brief Where the name of a LaTeX environment opened at i ends, or
     * npos when i does not open one.
     */
    pos latex_begin(pos i) const noexcept {
        if (!src_.match_ci(i, "\\begin{"))
            return npos;
        pos j = i + 7;
        while (j < n_ && (is_ascii_alnum(src_[j]) || src_[j] == '*'))
            ++j;
        return j > i + 7 && src_.at(j) == '}' ? j : npos;
    }

    /**
     * @brief Where the line that closes the LaTeX environment named at
     * [nb, ne) ends, searching [from, limit); npos when there is none.
     */
    pos latex_end(pos from, pos limit, pos nb, pos ne) const noexcept {
        const auto name = src_.view(nb, ne);
        for (pos q = src_.find(from, limit, '\\'); q != npos;
            q = src_.find(q + 1, limit, '\\')) {
            if (!src_.match_ci(q, "\\end{"))
                continue;
            const pos a = q + 5;
            if (n_ - a < name.size() + 1)
                return npos;
            bool same = true;
            for (std::size_t k = 0; k < name.size() && same; ++k)
                same = to_lower(src_[a + k]) == to_lower(name[k]);
            const pos close = a + static_cast<pos>(name.size());
            if (!same || src_[close] != '}')
                continue;
            const pos e = src_.skip_blank(close + 1);
            if (src_.at_eol(e) && e <= limit)
                return e;
        }
        return npos;
    }

    /**
     * @brief The colon that ends the key of a keyword whose key starts at
     * p, or npos.
     */
    pos keyword_colon(pos p) const noexcept {
        const pos e = token_end(p);
        for (pos k = e; k > p; --k) {
            if (src_[k - 1] == ':')
                return k - 1;
        }
        return npos;
    }

    pos footnote_label_end(pos p) const noexcept {
        if (!src_.match(p, "[fn:"))
            return npos;
        pos j = p + 4;
        while (j < n_ && (src_[j] == '-' || src_[j] == '_' ||
            src_.word_at(j)))
            j += src_.width(j);
        return j > p + 4 && src_.at(j) == ']' ? j : npos;
    }

    bool rule_line(pos i) const noexcept {
        pos j = i;
        while (src_.at(j) == '-')
            ++j;
        return j - i >= 5 && src_.at_eol(src_.skip_blank(j));
    }

    /**
     * @brief Whether the line at p is a table.el border: +---+---+.
     */
    bool border_line(pos p) const noexcept {
        pos j = src_.skip_blank(p);
        if (src_.at(j) != '+')
            return false;
        ++j;
        pos groups = 0;
        for (;;) {
            const pos dashes = j;
            while (src_.at(j) == '-')
                ++j;
            if (j == dashes || src_.at(j) != '+')
                break;
            ++j;
            ++groups;
        }
        return groups > 0 && src_.at_eol(src_.skip_blank(j));
    }

    bool table_el_line(pos p) const noexcept {
        const pos i = src_.skip_blank(p);
        return !src_.at_eol(i) && (src_[i] == '+' || src_[i] == '|');
    }

    bool table_el_at(pos p, pos limit) const noexcept {
        if (!border_line(p))
            return false;
        const pos next = src_.next_line(p);
        if (next >= limit)
            return false;
        pos l = next;
        pos last = p;
        while (l < limit && table_el_line(l)) {
            last = l;
            l = src_.next_line(l);
        }
        return l != next && border_line(last);
    }

    bool item_at(pos p) const noexcept {
        const pos i = src_.skip_blank(p);
        const char ch = src_.at(i);
        pos j = i + 1;
        if (ch == '*') {
            if (i == p)
                return false;
        } else if (is_digit(ch)) {
            while (is_digit(src_.at(j)))
                ++j;
            if (src_.at(j) != '.' && src_.at(j) != ')')
                return false;
            ++j;
        } else if (ch != '-' && ch != '+') {
            return false;
        }
        return is_blank(src_.at(j)) || src_.at_eol(j);
    }

    std::uint32_t column(pos p) const noexcept {
        std::uint32_t col = 0;
        for (; p < n_ && is_blank(src_[p]); ++p)
            col = src_[p] == '\t' ? (col / tab_width + 1) * tab_width
                                  : col + 1;
        return col;
    }

    item_fields item_line(pos p) const noexcept {
        item_fields f;
        const pos i = src_.skip_blank(p);
        const pos e = src_.eol(p);
        pos j = i + 1;
        if (is_digit(src_[i])) {
            while (is_digit(src_.at(j)))
                ++j;
            ++j;
            f.ordered = true;
        }
        f.bullet_tags = !f.ordered;
        pos k = src_.skip_blank(j, e);
        if (src_.match(k, "[@")) {
            pos m = k + 2;
            if (src_.match_ci(m, "start:"))
                m += 6;
            const pos value = m;
            while (is_digit(src_.at(m)))
                ++m;
            if (m == value && is_ascii_alpha(src_.at(m)))
                ++m;
            if (m > value && src_.at(m) == ']')
                k = src_.skip_blank(m + 1, e);
        }
        const char box = src_.at(k + 1);
        if (src_.at(k) == '[' && src_.at(k + 2) == ']' &&
            (box == ' ' || box == '-' || box == 'X' || box == 'x') &&
            (is_blank(src_.at(k + 3)) || src_.at_eol(k + 3))) {
            f.box = box == ' ' ? types::checkbox::off
                : box == 'X' ? types::checkbox::on
                : box == '-' ? types::checkbox::partial
                : types::checkbox::none;
            k = src_.skip_blank(k + 3, e);
        }
        f.after = k;
        for (pos q = e; q >= k + 3; --q) {
            if (src_[q - 1] == ':' && src_[q - 2] == ':' &&
                is_blank(src_[q - 3]) &&
                (q == e || is_blank(src_[q]))) {
                f.tagged = true;
                f.tag_begin = k;
                f.tag_end = q - 3;
                f.after = src_.skip_blank(q, e);
                break;
            }
        }
        return f;
    }

    list_entry* find_entry(structure st, pos p) noexcept {
        const auto first = entries_.begin() + st.first;
        const auto last = first + st.count;
        const auto it = std::lower_bound(first, last, p,
            [](const list_entry& e, pos v) { return e.begin < v; });
        return it != last && it->begin == p ? &*it : nullptr;
    }

    /**
     * @brief Where the block opened on the line at p with #+begin<suffix>
     * closes, searching up to limit; npos when it does not.
     */
    pos find_block_end(pos p, pos limit, std::string_view suffix) const
        noexcept {
        for (pos l = src_.next_line(p); l < limit && l > p;
            l = src_.next_line(l)) {
            const pos i = src_.skip_blank(l);
            if (src_.at(i) != '#' || !src_.match_ci(i, "#+end"))
                continue;
            const pos a = i + 5;
            if (n_ - a < suffix.size())
                continue;
            bool same = true;
            for (std::size_t k = 0; k < suffix.size() && same; ++k)
                same = to_lower(src_[a + k]) == to_lower(suffix[k]);
            const pos e = src_.skip_blank(a + static_cast<pos>(suffix.size()));
            if (same && src_.at_eol(e) && e <= limit)
                return l;
        }
        return npos;
    }

    structure build_structure(pos p, pos limit) {
        structure st{static_cast<std::uint32_t>(entries_.size()), 0};
        auto& open = stack_;
        open.clear();
        const auto close_to = [&](pos end) {
            for (const auto i : open)
                entries_[i].end = end;
            open.clear();
        };
        const auto after_text = [&](pos q) {
            return src_.next_line(src_.skip_ws_back(q, 0));
        };
        for (;;) {
            if (p >= limit) {
                close_to(after_text(p));
                break;
            }
            const pos next = src_.next_line(p);
            if (src_.blank_line(p)) {
                if (next < n_ && next > p && src_.blank_line(next) &&
                    src_.eol(next) < n_) {
                    close_to(p);
                    break;
                }
                p = next;
                continue;
            }
            const auto indent = column(p);
            if (item_at(p)) {
                while (!open.empty() &&
                    indent <= entries_[open.back()].indent) {
                    entries_[open.back()].end = p;
                    open.pop_back();
                }
                const auto f = item_line(p);
                open.push_back(static_cast<std::uint32_t>(entries_.size()));
                entries_.push_back({p, p, indent,
                    f.bullet_tags && f.tagged});
                p = next;
                continue;
            }
            const pos end = after_text(p);
            while (!open.empty() && indent <= entries_[open.back()].indent) {
                entries_[open.back()].end = end;
                open.pop_back();
            }
            if (open.empty())
                break;
            const pos i = src_.skip_blank(p);
            pos jump = npos;
            if (src_.match_ci(i, "#+begin:")) {
                jump = find_block_end(p, limit, ":");
            } else if (src_.match_ci(i, "#+begin_") &&
                !space_or_end(i + 8)) {
                jump = find_block_end(p, limit,
                    src_.view(i + 7, token_end(i + 7)));
            } else if (drawer_line(p)) {
                jump = find_end_line(p, limit);
            }
            p = jump == npos ? next : src_.next_line(jump);
        }
        st.count = static_cast<std::uint32_t>(entries_.size()) - st.first;
        return st;
    }

    parsed leaf(element_kind k, pos begin, pos before_blank, pos limit,
        element_id parent, pos cbeg = 0, pos cend = 0) {
        const pos end = src_.after_blank_lines(before_blank, limit);
        const auto id = c_.add_element(k, begin, end, cbeg, cend,
            src_.count_lines(before_blank, end), parent);
        return {id, k, end, cbeg, cend, {}, false};
    }

    /**
     * @brief Whether the line at l ends a paragraph that started before it.
     */
    bool separates(pos l, pos limit) const noexcept {
        if (heading_line_at(l) || footnote_label_end(l) != npos ||
            src_.match(l, "%%("))
            return true;
        const pos i = src_.skip_blank(l);
        if (src_.at_eol(i))
            return true;
        const char ch = src_[i];
        const auto item_like = [&](pos j) {
            return is_blank(src_.at(j)) || src_.at_eol(j);
        };
        switch (ch) {
        case '|':
            return true;
        case '+':
            return border_line(l) || item_like(i + 1);
        case '*':
            return item_like(i + 1);
        case '-':
            return rule_line(i) || item_like(i + 1);
        case '#':
            return hash_separates(l, i, limit);
        case ':':
            if (src_.at(i + 1) == ' ' || src_.at_eol(i + 1))
                return true;
            return drawer_line(l) &&
                find_end_line(src_.next_line(l), limit) != npos;
        case '\\': {
            const pos name_end = latex_begin(i);
            return name_end != npos &&
                latex_end(l, limit, i + 7, name_end) != npos;
        }
        default:
            break;
        }
        if (is_digit(ch)) {
            pos j = i;
            while (is_digit(src_.at(j)))
                ++j;
            return (src_.at(j) == '.' || src_.at(j) == ')') &&
                item_like(j + 1);
        }
        return clock_line(l);
    }

    bool hash_separates(pos l, pos i, pos limit) const noexcept {
        if (src_.at(i + 1) == ' ' || src_.at_eol(i + 1))
            return true;
        if (src_.at(i + 1) != '+')
            return false;
        const pos t = i + 2;
        const pos te = token_end(t);
        if (src_.match_ci(t, "begin_") && te > t + 6)
            return find_block_end(l, limit, src_.view(t + 5, te)) != npos;
        const pos e = src_.eol(l);
        pos bracket = npos;
        for (pos k = te; k > t + 1; --k) {
            if (src_[k - 1] == '[') {
                bracket = k - 1;
                break;
            }
        }
        if (bracket != npos && src_.find(bracket, e, "]:") != npos) {
            const auto key = src_.view(t, bracket);
            return equals_ci(key, "caption") || equals_ci(key, "results");
        }
        const pos colon = keyword_colon(t);
        return colon != npos && colon > t;
    }

    parsed paragraph(pos p, pos begin, pos limit, element_id parent) {
        pos before = limit;
        const pos first = src_.eol(p);
        pos l = first == p && src_.at_bol(p) ? p : src_.next_line(p);
        for (; l < limit; l = src_.next_line(l)) {
            if (separates(l, limit)) {
                before = l;
                break;
            }
            if (src_.next_line(l) == l)
                break;
        }
        const pos cend = src_.next_line(src_.skip_ws_back(before, p));
        return leaf(element_kind::paragraph, begin, before, limit, parent, p,
            cend);
    }

    parsed comment(pos p, pos limit, element_id parent) {
        pos l = src_.next_line(p);
        while (l < limit && comment_line(l))
            l = src_.next_line(l);
        return leaf(element_kind::comment, p, l, limit, parent);
    }

    /**
     * @brief A planning or clock line, which ends like no other element:
     * blank lines after it count, but trailing blanks on the last do not.
     */
    parsed line_element(element_kind k, pos p, pos limit,
        element_id parent) {
        const pos before = src_.next_line(p);
        const pos q = src_.skip_ws(before, limit);
        const pos back = src_.skip_blank_back(q, 0);
        const pos end = src_.at_bol(back) ? back : q;
        const auto id = c_.add_element(k, p, end, 0, 0,
            src_.count_lines(before, end), parent);
        return {id, k, end, 0, 0, {}, false};
    }

    parsed property_drawer(pos p, pos limit, element_id parent) {
        const pos cbeg = src_.next_line(p);
        const pos last = find_end_line(cbeg, limit);
        if (last == npos)
            return paragraph(p, p, limit, parent);
        return leaf(element_kind::property_drawer, p, src_.next_line(last),
            limit, parent, cbeg, last);
    }

    parsed node_property(pos p, element_id parent) {
        const pos i = src_.skip_blank(p);
        const pos te = token_end(i);
        const pos e = src_.eol(p);
        const pos end = src_.next_line(p);
        const auto id = c_.add_element(element_kind::node_property, p, end, 0,
            0, 0, parent);
        const pos vb = src_.skip_blank(te, e);
        auto& t = c_.t.properties;
        t.elements.values.push_back(id);
        t.names.values.push_back(c_.intern(
            te > i + 2 ? src_.view(i + 1, te - 1) : std::string_view{},
            name_case::upper));
        t.values.values.push_back({vb, src_.skip_blank_back(e, vb)});
        return {id, element_kind::node_property, end, 0, 0, {}, false};
    }

    parsed drawer(pos p, pos begin, pos limit, element_id parent) {
        const pos cbeg = src_.next_line(p);
        const pos last = find_end_line(cbeg, limit);
        if (last == npos)
            return paragraph(p, begin, limit, parent);
        return leaf(element_kind::drawer, begin, src_.next_line(last), limit,
            parent, cbeg, last);
    }

    parsed fixed_width(pos p, pos begin, pos limit, element_id parent) {
        pos l = p;
        const auto fixed = [&](pos q) {
            const pos i = src_.skip_blank(q);
            return src_.at(i) == ':' &&
                (src_.at(i + 1) == ' ' || src_.at_eol(i + 1));
        };
        while (l < limit && fixed(l)) {
            const pos next = src_.next_line(l);
            if (next == l)
                break;
            l = next;
        }
        const pos area_end = src_.at_bol(l) && l > 0 ? l - 1 : l;
        const pos end = src_.after_blank_lines(l, limit);
        const auto id = c_.add_element(element_kind::fixed_width, begin, end,
            0, 0, src_.count_lines(area_end, end), parent);
        return {id, element_kind::fixed_width, end, 0, 0, {}, false};
    }

    parsed latex_environment(pos p, pos begin, pos limit,
        element_id parent) {
        const pos i = src_.skip_blank(p);
        const pos close = latex_end(p, limit, i + 7, latex_begin(i));
        if (close == npos)
            return paragraph(p, begin, limit, parent);
        return leaf(element_kind::latex_environment, begin,
            src_.next_line(close), limit, parent);
    }

    void add_keyword(element_id id, std::string_view name, pos vb, pos e) {
        auto& t = c_.t.keywords;
        vb = src_.skip_blank(vb, e);
        t.elements.values.push_back(id);
        t.names.values.push_back(c_.intern(name, name_case::lower));
        t.values.values.push_back({vb, src_.skip_blank_back(e, vb)});
    }

    parsed keyword(pos p, pos begin, pos limit, element_id parent) {
        const pos i = src_.skip_blank(p) + 2;
        const pos colon = keyword_colon(i);
        const auto r = leaf(element_kind::keyword, begin, src_.next_line(p),
            limit, parent);
        add_keyword(r.id, src_.view(i, colon), colon + 1, src_.eol(p));
        return r;
    }

    parsed babel_call(pos p, pos begin, pos limit, element_id parent) {
        const pos i = src_.skip_blank(p) + 2;
        const auto r = leaf(element_kind::babel_call, begin,
            src_.next_line(p), limit, parent);
        add_keyword(r.id, "call", i + 5, src_.eol(p));
        return r;
    }

    void add_block(element_id id, types::name_id name, pos pb, pos pe,
        pos body_begin, pos body_end) {
        auto& t = c_.t.blocks;
        pb = src_.skip_blank(pb, pe);
        t.elements.values.push_back(id);
        t.names.values.push_back(name);
        t.parameters.values.push_back({pb, src_.skip_blank_back(pe, pb)});
        t.bodies.values.push_back({body_begin, body_end});
    }

    static element_kind block_kind(std::string_view name) noexcept {
        if (equals_ci(name, "src"))
            return element_kind::src_block;
        if (equals_ci(name, "example"))
            return element_kind::example_block;
        if (equals_ci(name, "quote"))
            return element_kind::quote_block;
        if (equals_ci(name, "export"))
            return element_kind::export_block;
        if (equals_ci(name, "center"))
            return element_kind::center_block;
        if (equals_ci(name, "comment"))
            return element_kind::comment_block;
        if (equals_ci(name, "verse"))
            return element_kind::verse_block;
        return element_kind::special_block;
    }

    parsed block(pos p, pos begin, pos limit, element_id parent) {
        const pos i = src_.skip_blank(p);
        const pos nb = i + 8;
        const pos ne = token_end(nb);
        const auto name = src_.view(nb, ne);
        const pos last = find_block_end(p, limit, src_.view(i + 7, ne));
        if (last == npos)
            return paragraph(p, begin, limit, parent);

        const auto kind = block_kind(name);
        const pos e = src_.eol(p);
        const pos body = src_.next_line(p);
        pos cbeg = 0;
        pos cend = 0;
        auto interned = types::name_id::none();
        pos pb = ne;
        switch (kind) {
        case element_kind::quote_block:
        case element_kind::center_block:
            cbeg = body;
            cend = last;
            break;
        case element_kind::special_block:
            cbeg = body;
            cend = last;
            interned = c_.intern(name);
            break;
        case element_kind::verse_block:
            cbeg = body;
            cend = last;
            break;
        case element_kind::src_block: {
            pos m = ne;
            while (m < e && src_[m] == ' ')
                ++m;
            if (m > ne && m < e && src_[m] != '\t') {
                pb = std::min(token_end(m), e);
                interned = c_.intern(src_.view(m, pb));
            }
            break;
        }
        case element_kind::export_block: {
            const pos m = src_.skip_blank(ne, e);
            if (m > ne && m < e) {
                pb = std::min(token_end(m), e);
                interned = c_.intern(src_.view(m, pb), name_case::lower);
            }
            break;
        }
        default:
            break;
        }
        const auto r = leaf(kind, begin, src_.next_line(last), limit, parent,
            cbeg, cend);
        add_block(r.id, interned, pb, e, body, last);
        return r;
    }

    parsed dynamic_block(pos p, pos begin, pos limit, element_id parent) {
        const pos last = find_block_end(p, limit, {});
        const pos colon_last = find_block_end(p, limit, ":");
        const pos close = std::min(last, colon_last);
        if (close == npos)
            return paragraph(p, begin, limit, parent);
        const pos nb = src_.skip_blank(src_.skip_blank(p) + 8);
        pos ne = nb;
        while (ne < n_ && src_.word_at(ne))
            ne += src_.width(ne);
        const pos body = src_.next_line(p);
        const auto r = leaf(element_kind::dynamic_block, begin,
            src_.next_line(close), limit, parent, body, close);
        add_block(r.id, c_.intern(src_.view(nb, ne)), ne, src_.eol(p), body,
            close);
        return r;
    }

    parsed footnote_definition(pos p, pos begin, pos limit,
        element_id parent) {
        pos end = limit;
        for (pos l = src_.next_line(p); l < limit && l > p;
            l = src_.next_line(l)) {
            if (heading_line_at(l)) {
                end = l;
                break;
            }
            if (footnote_label_end(l) != npos) {
                pos q = l;
                affiliated_fields f;
                while (q > 0) {
                    const pos previous = src_.bol(q - 1);
                    if (previous <= p || !affiliated_line(previous, f))
                        break;
                    q = previous;
                }
                end = q;
                break;
            }
            const pos next = src_.next_line(l);
            if (src_.blank_line(l) && next > l && next < n_ &&
                src_.blank_line(next) && src_.eol(next) < n_) {
                const pos q = src_.skip_ws(l, limit);
                end = q == limit ? limit : src_.bol(q);
                break;
            }
        }
        const pos label_end = footnote_label_end(p);
        const pos q = src_.skip_ws(label_end + 1, end);
        pos cbeg = 0;
        if (q < end)
            cbeg = src_.bol(q) == p ? q : src_.bol(q);
        const pos cend = src_.next_line(src_.skip_ws_back(end, 0));
        const bool filled = q < end;
        const auto id = c_.add_element(element_kind::footnote_definition,
            begin, end, filled ? cbeg : 0, filled ? cend : 0,
            src_.count_lines(cend, end), parent);
        return {id, element_kind::footnote_definition, end,
            filled ? cbeg : 0, filled ? cend : 0, {}, false};
    }

    parsed table(pos p, pos begin, pos limit, element_id parent, bool org) {
        pos l = p;
        while (l < limit) {
            const pos i = src_.skip_blank(l);
            if (src_.at_eol(i) ||
                (src_[i] != '|' && (org || src_[i] != '+')))
                break;
            const pos next = src_.next_line(l);
            if (next == l)
                break;
            l = next;
        }
        const pos table_end = std::min(l, limit);
        pos q = table_end;
        while (q < limit) {
            const pos i = src_.skip_blank(q);
            if (!src_.match_ci(i, "#+tblfm: "))
                break;
            const pos next = src_.next_line(q);
            if (next == q)
                break;
            q = next;
        }
        return leaf(org ? element_kind::table : element_kind::table_el, begin,
            q, limit, parent, org ? p : 0, org ? table_end : 0);
    }

    parsed table_row(pos p, element_id parent) {
        const pos i = src_.skip_blank(p);
        const pos end = src_.next_line(p);
        if (src_.at(i) == '|' && src_.at(i + 1) == '-') {
            const auto id = c_.add_element(element_kind::table_rule, p, end,
                0, 0, 0, parent);
            return {id, element_kind::table_rule, end, 0, 0, {}, false};
        }
        const pos cbeg = std::min(i + 1, src_.eol(p));
        const pos cend = src_.skip_blank_back(src_.eol(p), cbeg);
        const auto id = c_.add_element(element_kind::table_row, p, end, cbeg,
            cend, 0, parent);
        return {id, element_kind::table_row, end, cbeg, cend, {}, false};
    }

    parsed plain_list(pos p, pos begin, pos limit, element_id parent,
        structure st) {
        bool owns = false;
        auto* entry = st.count == 0 ? nullptr : find_entry(st, p);
        if (!entry) {
            st = build_structure(p, limit);
            owns = true;
            entry = find_entry(st, p);
        }
        const pos i = src_.skip_blank(p);
        const auto type = is_ascii_alnum(src_.at(i))
            ? types::list_type::ordered
            : entry->tagged ? types::list_type::descriptive
                            : types::list_type::unordered;
        pos stop = entry->end;
        for (const auto* e = find_entry(st, stop);
            e && e->indent == entry->indent; e = find_entry(st, stop))
            stop = e->end;
        const pos text_end = src_.skip_ws_back(stop, 0);
        const pos cend =
            src_.at_bol(text_end) ? text_end : src_.next_line(text_end);
        const pos q = src_.skip_ws(cend, limit);
        const pos end = q >= limit ? limit : src_.bol(q);
        const auto id = c_.add_element(element_kind::plain_list, begin, end,
            p, cend, src_.count_lines(cend, end), parent);
        c_.t.lists.elements.values.push_back(id);
        c_.t.lists.types.values.push_back(type);
        return {id, element_kind::plain_list, end, p, cend, st, owns};
    }

    parsed item(pos p, pos limit, element_id parent, structure st) {
        const auto f = item_line(p);
        const auto* entry = find_entry(st, p);
        pos end = entry ? entry->end : src_.next_line(p);
        end = std::min(limit, src_.at_bol(end) ? end : src_.next_line(end));
        const pos start = f.tagged && f.ordered ? f.tag_begin : f.after;
        const pos q = src_.skip_ws(start, end);
        const bool filled = q < end;
        pos cbeg = 0;
        pos cend = 0;
        if (filled) {
            cbeg = src_.bol(q) == p ? q : src_.bol(q);
            cend = src_.next_line(src_.skip_ws_back(end, 0));
        }
        const auto id = c_.add_element(element_kind::item, p, end, cbeg, cend,
            src_.count_lines(filled ? cend : p, end), parent);
        auto& t = c_.t.items;
        t.elements.values.push_back(id);
        t.checkboxes.values.push_back(f.box);
        t.tags.values.push_back(entry && entry->tagged
            ? types::source_range{f.tag_begin, f.tag_end}
            : types::source_range{});
        return {id, element_kind::item, end, cbeg, cend, {}, false};
    }

    context& c_;
    const text& src_;
    pos n_;
    std::vector<heading_line> headings_;
    std::vector<list_entry> entries_;
    std::vector<std::uint32_t> stack_;
};

}

void parse_elements(context& c) {
    element_parser(c).run();
}

}

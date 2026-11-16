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
#include <array>
#include <cstdint>
#include <string_view>
#include "context.hpp"
#include "entities.hpp"

namespace compass::ingest::org::detail {

namespace {

using types::element_id;
using types::element_kind;
using types::object_id;
using types::object_kind;

/**
 * @brief The object kinds a container may hold, one bit per kind.
 */
using restriction = std::uint32_t;

constexpr restriction bit(object_kind k) noexcept {
    return restriction{1} << static_cast<unsigned>(k);
}

template <typename... Kinds>
constexpr restriction bits(Kinds... kinds) noexcept {
    return (bit(kinds) | ...);
}

constexpr restriction minimal_set = bits(object_kind::bold, object_kind::code,
    object_kind::entity, object_kind::italic, object_kind::latex_fragment,
    object_kind::strike_through, object_kind::subscript,
    object_kind::superscript, object_kind::underline, object_kind::verbatim);

constexpr restriction standard_set = minimal_set | bits(object_kind::link,
    object_kind::timestamp, object_kind::footnote_reference,
    object_kind::inline_src_block, object_kind::inline_babel_call,
    object_kind::macro, object_kind::export_snippet,
    object_kind::statistics_cookie, object_kind::radio_target,
    object_kind::target, object_kind::line_break, object_kind::citation);

constexpr restriction title_set = standard_set & ~bit(object_kind::line_break);

constexpr restriction caption_set =
    standard_set & ~bit(object_kind::footnote_reference);

constexpr restriction link_set = minimal_set | bits(
    object_kind::export_snippet, object_kind::inline_babel_call,
    object_kind::inline_src_block, object_kind::macro,
    object_kind::statistics_cookie);

constexpr restriction cell_set = minimal_set | bits(object_kind::citation,
    object_kind::export_snippet, object_kind::footnote_reference,
    object_kind::link, object_kind::macro, object_kind::radio_target,
    object_kind::target, object_kind::timestamp);

/**
 * @brief The link types org registers out of the box.
 */
constexpr std::array<std::string_view, 23> link_types = {"id", "eww", "rmail",
    "mhe", "irc", "info", "gnus", "docview", "bibtex", "bbdb", "w3m", "doi",
    "file+sys", "file+emacs", "shell", "news", "mailto", "https", "http",
    "ftp", "help", "file", "elisp"};

/**
 * @brief Bytes that can start an object, so the lexer skips the rest.
 */
constexpr std::array<bool, 256> starts_object = [] {
    std::array<bool, 256> t{};
    for (const char ch : std::string_view{"_^*~=+/[@{<$\\"})
        t[static_cast<unsigned char>(ch)] = true;
    for (const auto type : link_types) {
        t[static_cast<unsigned char>(type[0])] = true;
        t[static_cast<unsigned char>(to_upper(type[0]))] = true;
    }
    for (const char ch : std::string_view{"cCsS"})
        t[static_cast<unsigned char>(ch)] = true;
    return t;
}();

class object_parser final {
public:
    explicit object_parser(context& c) noexcept
        : c_(c), src_(c.src) {}

    void run() {
        auto& t = c_.t;
        const auto caption = find_name("caption");
        for (std::uint32_t i = 0; i < t.affiliated.size(); ++i) {
            const types::affiliated_id a{i};
            t.affiliated.object_firsts[a] = next_object();
            if (!caption.is_none() && t.affiliated.names[a] == caption) {
                const auto v = t.affiliated.values[a];
                parse(v.begin, v.end, caption_set);
            }
        }

        std::uint32_t heading = 0;
        std::uint32_t item = 0;
        for (std::uint32_t i = 0; i < t.elements.size(); ++i) {
            const element_id e{i};
            t.elements.object_firsts[e] = next_object();
            const auto contents = t.elements.contents[e];
            switch (t.elements.kinds[e]) {
            case element_kind::heading:
                title(types::heading_id{heading++}, t.elements.ranges[e]);
                break;
            case element_kind::item: {
                const auto tag = t.items.tags[types::item_id{item++}];
                parse(tag.begin, tag.end, title_set);
                break;
            }
            case element_kind::paragraph:
                parse(contents.begin, contents.end, standard_set);
                break;
            case element_kind::table_row:
                cells(contents.begin, contents.end);
                break;
            case element_kind::planning:
            case element_kind::clock:
                timestamps(t.elements.ranges[e].begin);
                break;
            case element_kind::verse_block:
                parse(contents.begin, contents.end, standard_set);
                break;
            default:
                break;
            }
        }
    }

private:
    object_id next_object() const noexcept {
        return object_id{static_cast<std::uint32_t>(c_.t.objects.size())};
    }

    types::name_id find_name(std::string_view text) const noexcept {
        for (std::uint32_t i = 0; i < c_.t.names.size(); ++i) {
            if (c_.name(types::name_id{i}) == text)
                return types::name_id{i};
        }
        return types::name_id::none();
    }

    /**
     * @brief Appends an object row that ends after the blanks that follow
     * `after`, bounded by the container.
     */
    object_id add(object_kind k, pos begin, pos after, pos limit,
        pos cbeg = 0, pos cend = 0) {
        const pos end = src_.skip_blank(after, limit);
        return add_exact(k, begin, end, end - after, cbeg, cend);
    }

    object_id add_exact(object_kind k, pos begin, pos end, pos post_blank,
        pos cbeg, pos cend) {
        auto& o = c_.t.objects;
        const auto id = next_object();
        o.kinds.values.push_back(k);
        o.post_blanks.values.push_back(context::saturate(post_blank));
        o.ranges.values.push_back({begin, end});
        o.contents.values.push_back(
            cbeg < cend ? types::source_range{cbeg, cend}
                        : types::source_range{});
        o.subtree_ends.values.push_back(id.next());
        return id;
    }

    void close(object_id id) {
        c_.t.objects.subtree_ends[id] = next_object();
    }

    /**
     * @brief Parses the objects of [beg, end), which org-element narrows
     * to: nothing outside the range is seen.
     */
    void parse(pos beg, pos end, restriction allowed) {
        pos p = beg;
        while (p < end) {
            if (!starts_object[static_cast<unsigned char>(src_[p])]) {
                ++p;
                continue;
            }
            const pos after = object_at(p, beg, end, allowed);
            p = after == npos ? p + 1 : std::max(after, p + 1);
        }
    }

    static bool allows(restriction allowed, object_kind k) noexcept {
        return (allowed & bit(k)) != 0;
    }

    /**
     * @brief Parses the object that starts at p, if there is one, and
     * returns where it ends; npos when p starts no object.
     */
    pos object_at(pos p, pos beg, pos end, restriction allowed) {
        const char ch = src_[p];
        const char next = p + 1 < end ? src_[p + 1] : '\0';
        const bool next_solid = p + 1 < end && !src_.space_at(p + 1);
        switch (ch) {
        case '^':
            if (p + 1 < end && script_start(p + 1) &&
                allows(allowed, object_kind::superscript))
                return script(p, beg, end, object_kind::superscript);
            return npos;
        case '_': {
            if (!next_solid)
                return npos;
            pos r = npos;
            if (allows(allowed, object_kind::underline))
                r = emphasis(p, beg, end, object_kind::underline);
            if (r == npos && allows(allowed, object_kind::subscript))
                r = script(p, beg, end, object_kind::subscript);
            return r;
        }
        case '*':
            return next_solid && allows(allowed, object_kind::bold)
                ? emphasis(p, beg, end, object_kind::bold) : npos;
        case '/':
            return next_solid && allows(allowed, object_kind::italic)
                ? emphasis(p, beg, end, object_kind::italic) : npos;
        case '~':
            return next_solid && allows(allowed, object_kind::code)
                ? emphasis(p, beg, end, object_kind::code) : npos;
        case '=':
            return next_solid && allows(allowed, object_kind::verbatim)
                ? emphasis(p, beg, end, object_kind::verbatim) : npos;
        case '+':
            return next_solid && allows(allowed, object_kind::strike_through)
                ? emphasis(p, beg, end, object_kind::strike_through) : npos;
        case '@':
            return next == '@' && allows(allowed, object_kind::export_snippet)
                ? export_snippet(p, end) : npos;
        case '{':
            return src_.match(p, "{{{") && p + 3 <= end &&
                allows(allowed, object_kind::macro) ? macro(p, end) : npos;
        case '$':
            return allows(allowed, object_kind::latex_fragment)
                ? dollar_fragment(p, beg, end) : npos;
        case '<':
            return angle(p, end, allowed);
        case '\\':
            return backslash(p, beg, end, allowed);
        case '[':
            return bracket(p, end, allowed);
        default:
            return word(p, beg, end, allowed);
        }
    }

    /**
     * @brief Whether the character at p may follow the caret of a
     * superscript, as the object lexer of org-element requires.
     */
    bool script_start(pos p) const noexcept {
        const char ch = src_[p];
        return ch == '-' || ch == '{' || ch == '(' || ch == '*' ||
            ch == '+' || ch == '.' || ch == ',' || src_.alnum_at(p);
    }

    /**
     * @brief Whether the character before p lets markup open at p.
     */
    bool opens_after(pos p, pos beg) const noexcept {
        if (p == beg || src_[p - 1] == '\n')
            return true;
        const char b = src_[p - 1];
        if (is_ascii(b))
            return b == ' ' || b == '\t' || b == '\r' || b == '\f' ||
                b == '-' || b == '(' || b == '\'' || b == '"' || b == '{';
        return src_.space_at(src_.previous(p));
    }

    /**
     * @brief Whether markup may close before p.
     */
    bool closes_before(pos p, pos end) const noexcept {
        if (p >= end || src_.at_eol(p))
            return true;
        const char a = src_[p];
        if (!is_ascii(a))
            return src_.space_at(p);
        switch (a) {
        case ' ': case '\t': case '\r': case '\f': case '\n':
        case '-': case '.': case ',': case ';': case ':': case '!':
        case '?': case '\'': case '"': case ')': case '}': case '\\':
        case '[':
            return true;
        default:
            return false;
        }
    }

    bool solid_before(pos p) const noexcept {
        const char b = src_[p - 1];
        return is_ascii(b) ? !src_.space_at(p - 1)
                           : !src_.space_at(src_.previous(p));
    }

    static char mark_of(object_kind k) noexcept {
        switch (k) {
        case object_kind::bold: return '*';
        case object_kind::italic: return '/';
        case object_kind::underline: return '_';
        case object_kind::strike_through: return '+';
        case object_kind::code: return '~';
        default: return '=';
        }
    }

    pos emphasis(pos p, pos beg, pos end, object_kind k) {
        if (!opens_after(p, beg))
            return npos;
        const char mark = mark_of(k);
        pos close = npos;
        for (pos i = src_.find(p + 2, end, mark); i != npos;
            i = src_.find(i + 1, end, mark)) {
            if (solid_before(i) && closes_before(i + 1, end)) {
                close = i;
                break;
            }
        }
        if (close == npos)
            return npos;
        if (k == object_kind::code || k == object_kind::verbatim) {
            const auto id = add(k, p, close + 1, end);
            return c_.t.objects.ranges[id].end;
        }
        const auto id = add(k, p, close + 1, end, p + 1, close);
        parse(p + 1, close, standard_set);
        this->close(id);
        return c_.t.objects.ranges[id].end;
    }

    /**
     * @brief The position after the bracket that balances the one at p,
     * counting only that pair and nesting at most `depth` deep; npos when
     * there is none in [p, end).
     */
    pos balanced(pos p, pos end, char open, char shut,
        std::uint32_t depth) const noexcept {
        std::uint32_t level = 0;
        for (pos i = p; i < end; ++i) {
            if (src_[i] == open) {
                if (++level > depth)
                    return npos;
            } else if (src_[i] == shut) {
                if (--level == 0)
                    return i + 1;
            }
        }
        return npos;
    }

    pos script(pos p, pos beg, pos end, object_kind k) {
        if (p == beg || src_[p - 1] == '\n' || !solid_before(p) ||
            p + 1 >= end)
            return npos;
        const pos r = p + 1;
        const char ch = src_[r];
        pos cbeg = r;
        pos cend = npos;
        pos after = npos;
        if (ch == '{') {
            after = balanced(r, end, '{', '}', 3);
            if (after == npos)
                return npos;
            cbeg = r + 1;
            cend = after - 1;
        } else if (ch == '(') {
            after = balanced(r, end, '(', ')', 3);
            if (after == npos)
                return npos;
            cend = after;
        } else if (ch == '*') {
            after = cend = r + 1;
        } else {
            pos i = ch == '+' || ch == '-' ? r + 1 : r;
            pos j = i;
            pos last = npos;
            while (j < end) {
                const char x = src_[j];
                if (src_.alnum_at(j)) {
                    j += src_.width(j);
                    last = j;
                } else if (x == '.' || x == ',' || x == '\\') {
                    ++j;
                } else {
                    break;
                }
            }
            if (last == npos)
                return npos;
            after = cend = std::min(last, end);
        }
        const auto id = add(k, p, after, end, cbeg, cend);
        parse(cbeg, cend, standard_set);
        close(id);
        return c_.t.objects.ranges[id].end;
    }

    pos export_snippet(pos p, pos end) {
        pos j = p + 2;
        while (j < end && (is_ascii_alnum(src_[j]) || src_[j] == '-'))
            ++j;
        if (j == p + 2 || j >= end || src_[j] != ':')
            return npos;
        const pos close = src_.find(j + 1, end, "@@");
        if (close == npos)
            return npos;
        const auto id = add(object_kind::export_snippet, p, close + 2, end);
        return c_.t.objects.ranges[id].end;
    }

    pos macro(pos p, pos end) {
        pos j = p + 3;
        if (j >= end || !is_ascii_alpha(src_[j]))
            return npos;
        while (j < end && (is_ascii_alnum(src_[j]) || src_[j] == '-' ||
            src_[j] == '_'))
            ++j;
        pos after = npos;
        if (src_.match(j, "}}}") && j + 3 <= end) {
            after = j + 3;
        } else if (j < end && src_[j] == '(') {
            const pos close = src_.find(j + 1, end, ")}}}");
            if (close != npos)
                after = close + 4;
        }
        if (after == npos)
            return npos;
        const auto id = add(object_kind::macro, p, after, end);
        return c_.t.objects.ranges[id].end;
    }

    pos dollar_fragment(pos p, pos beg, pos end) {
        pos after = npos;
        if (p + 1 < end && src_[p + 1] == '$') {
            const pos close = src_.find(p + 2, end, "$$");
            if (close != npos)
                after = close + 2;
        } else {
            if (p > beg && src_[p - 1] == '$')
                return npos;
            if (p + 1 < end) {
                const char a = src_[p + 1];
                if (a == ' ' || a == '\t' || a == '\n' || a == ',' ||
                    a == '.' || a == ';')
                    return npos;
            }
            const pos close = src_.find(p + 1, end, '$');
            if (close == npos)
                return npos;
            const char b = src_[close - 1];
            if (b == ' ' || b == '\t' || b == '\n' || b == ',' || b == '.')
                return npos;
            const pos q = close + 1;
            if (q < end && !src_.at_eol(q)) {
                const char a = src_[q];
                const bool ok = is_ascii(a)
                    ? (a == '\'' ||
                        std::string_view{". ()\""}.find(syntax_of(a)) !=
                            std::string_view::npos)
                    : !is_wide_word(src_.code_point(q));
                if (!ok)
                    return npos;
            }
            after = q;
        }
        if (after == npos)
            return npos;
        const auto id = add(object_kind::latex_fragment, p, after, end);
        return c_.t.objects.ranges[id].end;
    }

    /**
     * @brief Where a timestamp of the form [date ...] or <date ...> that
     * starts at p ends, or npos.
     */
    pos plain_timestamp(pos p, pos end) const noexcept {
        if (p + 12 > end || (src_[p] != '[' && src_[p] != '<'))
            return npos;
        for (const pos d : {1u, 2u, 3u, 4u, 6u, 7u, 9u, 10u}) {
            if (!is_digit(src_[p + d]))
                return npos;
        }
        if (src_[p + 5] != '-' || src_[p + 8] != '-')
            return npos;
        const char a = src_[p + 11];
        if (a == ']' || a == '>')
            return p + 12;
        if (a != ' ')
            return npos;
        const pos e = std::min(src_.eol(p), end);
        for (pos i = p + 12; i < e; ++i) {
            if (src_[i] == ']' || src_[i] == '>')
                return i + 1;
        }
        return npos;
    }

    bool timestamp_at(pos p, pos end) const noexcept {
        if (plain_timestamp(p, end) != npos)
            return true;
        if (src_[p] != '<')
            return false;
        const pos e = std::min(src_.eol(p), end);
        const pos close = src_.find(p + 1, e, '>');
        if (close == npos)
            return false;
        if (src_.match(p, "<%%(")) {
            return src_.find(p + 4, close, ')') != npos;
        }
        if (!is_digit(src_.at(p + 1)))
            return false;
        const char unit = src_[close - 1];
        if (unit != 'd' && unit != 'w' && unit != 'm' && unit != 'y')
            return false;
        pos i = close - 1;
        while (i > p && is_digit(src_[i - 1]))
            --i;
        return i < close - 1 && i > p + 1 && src_[i - 1] == '+';
    }

    pos timestamp(pos p, pos end) {
        if (!timestamp_at(p, end))
            return npos;
        const pos e = std::min(src_.eol(p), end);
        pos after = npos;
        for (pos i = p + 1; i < e; ++i) {
            if (src_[i] == ']' || src_[i] == '>') {
                after = i + 1;
                break;
            }
        }
        if (after == npos)
            return npos;
        if (src_.match(after, "--") && after + 2 < end) {
            const pos second = plain_timestamp(after + 2, end);
            if (second != npos)
                after = second;
        }
        const auto id = add(object_kind::timestamp, p, after, end);
        return c_.t.objects.ranges[id].end;
    }

    void timestamps(pos line) {
        const pos e = src_.eol(line);
        for (pos i = line; i < e; ++i) {
            if (src_[i] != '[' && src_[i] != '<')
                continue;
            const pos after = timestamp(i, e);
            if (after != npos)
                i = after - 1;
        }
    }

    /**
     * @brief Where the text of a target that opens with `marks` angle
     * brackets at p ends, or npos.
     */
    pos target_text_end(pos p, pos end, pos marks) const noexcept {
        const pos b = p + marks;
        pos i = b;
        while (i < end && src_[i] != '<' && src_[i] != '>' &&
            src_[i] != '\n')
            ++i;
        if (i == b || i + marks > end || is_blank(src_[b]) ||
            is_blank(src_[i - 1]))
            return npos;
        for (pos k = 0; k < marks; ++k) {
            if (src_[i + k] != '>')
                return npos;
        }
        return i;
    }

    pos angle(pos p, pos end, restriction allowed) {
        if (p + 1 < end && src_[p + 1] == '<') {
            if (allows(allowed, object_kind::radio_target) &&
                p + 2 < end && src_[p + 2] == '<') {
                const pos te = target_text_end(p, end, 3);
                if (te != npos) {
                    const auto id = add(object_kind::radio_target, p, te + 3,
                        end, p + 3, te);
                    parse(p + 3, te, minimal_set);
                    close(id);
                    return c_.t.objects.ranges[id].end;
                }
            }
            if (allows(allowed, object_kind::target)) {
                const pos te = target_text_end(p, end, 2);
                if (te != npos) {
                    const auto id =
                        add(object_kind::target, p, te + 2, end);
                    return c_.t.objects.ranges[id].end;
                }
            }
            return npos;
        }
        if (allows(allowed, object_kind::timestamp)) {
            const pos r = timestamp(p, end);
            if (r != npos)
                return r;
        }
        if (allows(allowed, object_kind::link))
            return angle_link(p, end);
        return npos;
    }

    pos entity(pos p, pos end) {
        const pos b = p + 1;
        pos name_end = npos;
        if (b < end && src_[b] == '_') {
            pos j = b + 1;
            while (j < end && src_[j] == ' ')
                ++j;
            if (j == b + 1 || j - b - 1 > 20)
                return npos;
            name_end = j;
        } else {
            const auto follows = [&](pos q) {
                return q >= end || src_.at_eol(q) ||
                    (src_[q] == '{' && q + 1 < end && src_[q + 1] == '}') ||
                    !src_.alpha_at(q);
            };
            const auto rest = src_.view(b, end);
            pos fixed = 0;
            if (rest.starts_with("there4")) {
                fixed = 6;
            } else if (rest.starts_with("sup") && rest.size() > 3 &&
                rest[3] >= '1' && rest[3] <= '3') {
                fixed = 4;
            } else if (rest.starts_with("frac") && rest.size() > 5 &&
                (rest[4] == '1' || rest[4] == '3') &&
                (rest[5] == '2' || rest[5] == '4')) {
                fixed = 6;
            }
            if (fixed != 0 && follows(b + fixed)) {
                name_end = b + fixed;
            } else {
                pos j = b;
                while (j < end && is_ascii_alpha(src_[j]))
                    ++j;
                if (j == b || !follows(j))
                    return npos;
                name_end = j;
            }
            if (!is_entity(src_.view(b, name_end)))
                return npos;
        }
        pos after = name_end;
        if (src_.match(after, "{}") && after + 2 <= end && src_[b] != '_')
            after += 2;
        const auto id = add(object_kind::entity, p, after, end);
        return c_.t.objects.ranges[id].end;
    }

    pos backslash_fragment(pos p, pos end) {
        if (p + 1 >= end)
            return npos;
        pos after = npos;
        const char a = src_[p + 1];
        if (a == '(' || a == '[') {
            const pos close = src_.find(p + 2, end, a == '(' ? "\\)" : "\\]");
            if (close != npos)
                after = close + 2;
        } else if (is_ascii_alpha(a)) {
            pos j = p + 1;
            while (j < end && is_ascii_alpha(src_[j]))
                ++j;
            if (j < end && src_[j] == '*')
                ++j;
            for (;;) {
                if (j >= end)
                    break;
                const char open = src_[j];
                if (open != '[' && open != '{')
                    break;
                pos k = j + 1;
                const char shut = open == '[' ? ']' : '}';
                while (k < end && src_[k] != '\n' && src_[k] != '{' &&
                    src_[k] != '}' &&
                    (open == '{' || (src_[k] != '[' && src_[k] != ']')))
                    ++k;
                if (k >= end || src_[k] != shut)
                    break;
                j = k + 1;
            }
            after = j;
        }
        if (after == npos)
            return npos;
        const auto id = add(object_kind::latex_fragment, p, after, end);
        return c_.t.objects.ranges[id].end;
    }

    pos backslash(pos p, pos beg, pos end, restriction allowed) {
        if (p + 1 >= end)
            return npos;
        const char a = src_[p + 1];
        if (a == '\\') {
            const pos q = src_.skip_blank(p + 2, end);
            if ((q < end && !src_.at_eol(q)) ||
                !allows(allowed, object_kind::line_break) ||
                (p > beg && src_[p - 1] == '\\'))
                return npos;
            const pos stop = std::min(src_.next_line(p), end);
            add_exact(object_kind::line_break, p, stop, 0, 0, 0);
            return stop;
        }
        const bool spaced_underscore =
            a == '_' && p + 2 < end && src_[p + 2] == ' ';
        if (!is_ascii_alpha(a) && a != '[' && a != '(' && !spaced_underscore)
            return npos;
        if (allows(allowed, object_kind::entity)) {
            const pos r = entity(p, end);
            if (r != npos)
                return r;
        }
        if (allows(allowed, object_kind::latex_fragment))
            return backslash_fragment(p, end);
        return npos;
    }

    pos statistics_cookie(pos p, pos end) {
        pos j = p + 1;
        while (j < end && is_digit(src_[j]))
            ++j;
        if (j >= end)
            return npos;
        if (src_[j] == '%') {
            ++j;
        } else if (src_[j] == '/') {
            ++j;
            while (j < end && is_digit(src_[j]))
                ++j;
        } else {
            return npos;
        }
        if (j >= end || src_[j] != ']')
            return npos;
        const auto id = add(object_kind::statistics_cookie, p, j + 1, end);
        return c_.t.objects.ranges[id].end;
    }

    pos footnote_reference(pos p, pos end) {
        pos j = p + 4;
        while (j < end && (src_[j] == '-' || src_[j] == '_' ||
            src_.word_at(j)))
            j += src_.width(j);
        if (j >= end)
            return npos;
        const bool inline_definition = src_[j] == ':';
        if (!inline_definition && (src_[j] != ']' || j == p + 4))
            return npos;
        const pos after = balanced(p, end, '[', ']', 0xFFFF'FFFF);
        if (after == npos)
            return npos;
        if (!inline_definition) {
            const auto id =
                add(object_kind::footnote_reference, p, after, end);
            return c_.t.objects.ranges[id].end;
        }
        const auto id = add(object_kind::footnote_reference, p, after, end,
            j + 1, after - 1);
        parse(j + 1, after - 1, standard_set);
        close(id);
        return c_.t.objects.ranges[id].end;
    }

    pos bracket(pos p, pos end, restriction allowed) {
        if (p + 1 >= end)
            return npos;
        const char a = src_[p + 1];
        if (a == '[')
            return allows(allowed, object_kind::link)
                ? bracket_link(p, end) : npos;
        if (a == 'f') {
            return src_.match(p, "[fn:") && p + 4 <= end &&
                allows(allowed, object_kind::footnote_reference)
                ? footnote_reference(p, end) : npos;
        }
        if (a == '%' || a == '/') {
            return allows(allowed, object_kind::statistics_cookie)
                ? statistics_cookie(p, end) : npos;
        }
        if (!is_digit(a))
            return npos;
        if (allows(allowed, object_kind::timestamp)) {
            const pos r = timestamp(p, end);
            if (r != npos)
                return r;
        }
        return allows(allowed, object_kind::statistics_cookie)
            ? statistics_cookie(p, end) : npos;
    }

    /**
     * @brief The registered link type written at [p, end) before a colon,
     * or empty.
     */
    std::string_view link_type_at(pos p, pos end) const noexcept {
        for (const auto type : link_types) {
            const pos colon = p + static_cast<pos>(type.size());
            if (colon < end && src_[colon] == ':' && src_.match_ci(p, type))
                return src_.view(p, colon);
        }
        return {};
    }

    /**
     * @brief Appends a link row. The type is interned as written, except
     * that file in any case, file+sys and file+emacs are all file.
     */
    void add_link(object_id id, std::string_view type,
        types::link_format format, pos pb, pos pe) {
        auto& l = c_.t.links;
        const bool file = type.size() >= 4 && to_lower(type[0]) == 'f' &&
            to_lower(type[1]) == 'i' && to_lower(type[2]) == 'l' &&
            to_lower(type[3]) == 'e' && (type.size() == 4 || type[4] == '+');
        l.objects.values.push_back(id);
        l.types.values.push_back(c_.intern(file ? "file" : type));
        l.formats.values.push_back(format);
        l.paths.values.push_back({pb, pe});
    }

    pos bracket_link(pos p, pos end) {
        pos j = p + 2;
        while (j < end) {
            const char x = src_[j];
            if (x == ']' || x == '[')
                break;
            if (x != '\\') {
                ++j;
                continue;
            }
            pos k = j;
            while (k < end && src_[k] == '\\')
                ++k;
            const bool escapes = (k - j) % 2 == 1;
            if (k < end && (src_[k] == '[' || src_[k] == ']')) {
                j = escapes ? k + 1 : k;
            } else if (k < end) {
                j = k + 1;
            } else {
                j = k;
            }
        }
        if (j == p + 2 || j >= end || src_[j] != ']')
            return npos;
        const pos path_end = j;
        pos after = npos;
        pos cbeg = 0;
        pos cend = 0;
        if (j + 1 < end && src_[j + 1] == ']') {
            after = j + 2;
        } else if (j + 1 < end && src_[j + 1] == '[') {
            const pos close = src_.find(j + 3, end, "]]");
            if (close == npos)
                return npos;
            cbeg = j + 2;
            cend = close;
            after = close + 2;
        } else {
            return npos;
        }

        const auto id = add(object_kind::link, p, after, end, cbeg, cend);
        const pos pb = p + 2;
        const auto path = src_.view(pb, path_end);
        const auto registered = link_type_at(pb, path_end);
        if (path[0] == '/' || path.starts_with("./") ||
            path.starts_with("../") || path == "~" ||
            path.starts_with("~/")) {
            add_link(id, "file", types::link_format::bracket, pb,
                path_end);
        } else if (!registered.empty()) {
            add_link(id, registered, types::link_format::bracket,
                pb + static_cast<pos>(registered.size()) + 1, path_end);
        } else if (path.size() >= 2 && path.front() == '(' &&
            path.back() == ')') {
            add_link(id, "coderef", types::link_format::bracket,
                pb + 1, path_end - 1);
        } else if (path[0] == '#') {
            add_link(id, "custom-id", types::link_format::bracket,
                pb + 1, path_end);
        } else if (const pos colon = scheme_colon(pb, path_end);
            colon != npos) {
            add_link(id, src_.view(pb, colon),
                types::link_format::bracket, colon + 1, path_end);
        } else {
            add_link(id, "fuzzy", types::link_format::bracket, pb,
                path_end);
        }
        parse(cbeg, cend, link_set);
        close(id);
        return c_.t.objects.ranges[id].end;
    }

    /**
     * @brief The colon after a scheme org does not register, such as the
     * one in proj:name, or npos when [b, e) does not start with a scheme.
     */
    pos scheme_colon(pos b, pos e) const noexcept {
        if (!is_ascii_alpha(src_[b]))
            return npos;
        pos j = b + 1;
        while (j < e && (is_ascii_alnum(src_[j]) || src_[j] == '-' ||
            src_[j] == '+' || src_[j] == '.' || src_[j] == '_'))
            ++j;
        if (j + 1 >= e || src_[j] != ':')
            return npos;
        const char a = src_[j + 1];
        return a == ':' || is_ws(a) ? npos : j;
    }

    static bool path_char(char x) noexcept {
        return x != '[' && x != ']' && x != ' ' && x != '\t' && x != '\n' &&
            x != '(' && x != ')' && x != '<' && x != '>';
    }

    static bool opener(char x) noexcept {
        return x == '<' || x == '(' || x == '[';
    }

    static bool closer(char x) noexcept {
        return x == ']' || x == ')' || x == '>';
    }

    /**
     * @brief Where a parenthesised group of a plain link path that opens
     * at p ends, or npos.
     */
    pos path_group(pos p, pos end) const noexcept {
        pos j = p + 1;
        while (j < end) {
            if (path_char(src_[j])) {
                ++j;
            } else if (opener(src_[j])) {
                pos k = j + 1;
                while (k < end && path_char(src_[k]))
                    ++k;
                if (k >= end || !closer(src_[k]))
                    break;
                j = k + 1;
            } else {
                break;
            }
        }
        return j < end && closer(src_[j]) ? j + 1 : npos;
    }

    /**
     * @brief Where the path of a plain link that starts at p ends, or npos.
     */
    pos plain_path_end(pos p, pos end) const noexcept {
        pos j = p;
        pos last = npos;
        pos count = 0;
        while (j < end) {
            if (path_char(src_[j])) {
                const pos w = src_.width(j);
                ++count;
                if (count >= 2 && (src_[j] == '/' || !src_.punct_at(j)))
                    last = j + w;
                j += w;
            } else if (opener(src_[j])) {
                const pos g = path_group(j, end);
                if (g == npos)
                    break;
                ++count;
                if (count >= 2)
                    last = g;
                j = g;
            } else {
                break;
            }
        }
        return last;
    }

    bool word_start(pos p, pos beg) const noexcept {
        if (p == beg)
            return true;
        const char b = src_[p - 1];
        return is_ascii(b) ? syntax_of(b) != 'w'
                           : !src_.word_at(src_.previous(p));
    }

    pos plain_link(pos p, pos beg, pos end) {
        if (!word_start(p, beg))
            return npos;
        const auto type = link_type_at(p, end);
        if (type.empty())
            return npos;
        const pos pb = p + static_cast<pos>(type.size()) + 1;
        const pos pe = plain_path_end(pb, end);
        if (pe == npos)
            return npos;
        const auto id = add(object_kind::link, p, pe, end);
        add_link(id, type, types::link_format::plain, pb, pe);
        return c_.t.objects.ranges[id].end;
    }

    pos angle_link(pos p, pos end) {
        const auto type = link_type_at(p + 1, end);
        if (type.empty())
            return npos;
        const pos pb = p + 2 + static_cast<pos>(type.size());
        pos j = pb;
        while (j < end && src_[j] != '>') {
            if (src_[j] == '\n') {
                const pos k = src_.skip_blank(j + 1, end);
                if (k >= end || src_[k] == '\n' || src_[k] == '>')
                    return npos;
            }
            ++j;
        }
        if (j >= end)
            return npos;
        const auto id = add(object_kind::link, p, j + 1, end);
        add_link(id, type, types::link_format::angle, pb, j);
        return c_.t.objects.ranges[id].end;
    }

    /**
     * @brief An inline source block or an inline babel call: a name, then
     * an optional header in square brackets and a body in `open` brackets.
     */
    pos inline_code(pos p, pos beg, pos end, object_kind k, pos prefix,
        char open, char shut) {
        if (!word_start(p, beg))
            return npos;
        pos j = p + prefix;
        while (j < end && !is_blank(src_[j]) && src_[j] != '\n' &&
            src_[j] != '[' && src_[j] != open)
            ++j;
        if (j == p + prefix || j >= end)
            return npos;
        if (src_[j] == '[') {
            j = balanced(j, end, '[', ']', 0xFFFF'FFFF);
            if (j == npos || j >= end)
                return npos;
        }
        if (src_[j] != open)
            return npos;
        j = balanced(j, end, open, shut, 0xFFFF'FFFF);
        if (j == npos)
            return npos;
        if (k == object_kind::inline_babel_call && j < end &&
            src_[j] == '[') {
            const pos tail = balanced(j, end, '[', ']', 0xFFFF'FFFF);
            if (tail != npos)
                j = tail;
        }
        const auto id = add(k, p, j, end);
        return c_.t.objects.ranges[id].end;
    }

    pos word(pos p, pos beg, pos end, restriction allowed) {
        if (src_.match(p, "call_") &&
            allows(allowed, object_kind::inline_babel_call)) {
            const pos r = inline_code(p, beg, end,
                object_kind::inline_babel_call, 5, '(', ')');
            if (r != npos)
                return r;
        }
        if (src_.match(p, "src_") &&
            allows(allowed, object_kind::inline_src_block)) {
            const pos r = inline_code(p, beg, end,
                object_kind::inline_src_block, 4, '{', '}');
            if (r != npos)
                return r;
        }
        return allows(allowed, object_kind::link) ? plain_link(p, beg, end)
                                                  : npos;
    }

    void cells(pos cbeg, pos cend) {
        pos p = cbeg;
        while (p < cend) {
            const pos i = src_.skip_blank(p, cend);
            const pos bar = src_.find(i, cend, '|');
            const pos stop = bar == npos ? cend : bar;
            const pos end = bar == npos ? cend : bar + 1;
            const pos ce = src_.skip_blank_back(stop, i);
            const auto id =
                add_exact(object_kind::table_cell, p, end, 0, i, ce);
            parse(i, ce, cell_set);
            close(id);
            p = end;
        }
    }

    bool tag_char(pos p) const noexcept {
        const char ch = src_[p];
        if (!is_ascii(ch))
            return true;
        return is_ascii_alnum(ch) || ch == '_' || ch == '@' || ch == '#' ||
            ch == '%' || ch == ':';
    }

    void title(types::heading_id h, types::source_range range) {
        auto& t = c_.t.headings;
        const pos e = src_.eol(range.begin);
        pos p = range.begin;
        while (p < e && src_[p] == '*')
            ++p;
        p = src_.skip_blank(p, e);

        pos word_end = p;
        while (word_end < e && src_[word_end] != ' ')
            ++word_end;
        const auto first = src_.view(p, word_end);
        for (const auto& w : c_.vocabulary) {
            if (w.text != first)
                continue;
            t.todos[h] = c_.intern(first);
            t.todo_types[h] = w.type;
            p = src_.skip_blank(std::min(word_end + 1, e), e);
            break;
        }

        if (src_.match(p, "[#") && p + 2 < e) {
            const pos w = src_.width(p + 2);
            if (p + 2 + w < e && src_[p + 2 + w] == ']') {
                t.priorities[h] = static_cast<std::uint8_t>(src_[p + 2]);
                p = src_.skip_blank(p + 3 + w, e);
            }
        }

        if (src_.match(p, "COMMENT") && p + 7 <= e &&
            (p + 7 == e || src_[p + 7] == ' ')) {
            t.comments[h] = types::comment_state::commented;
            p = src_.skip_blank(std::min(p + 8, e), e);
        }

        const pos title_start = p;
        pos title_end = e;
        const pos trimmed = src_.skip_blank_back(e, title_start);
        t.tag_firsts[h] = types::tag_id{
            static_cast<std::uint32_t>(c_.t.heading_tags.size())};
        if (trimmed > title_start && src_[trimmed - 1] == ':') {
            pos run = trimmed;
            while (run > title_start && tag_char(run - 1))
                --run;
            pos colon = run;
            while (colon < trimmed && src_[colon] != ':')
                ++colon;
            if (trimmed - colon >= 3) {
                title_end = colon;
                pos b = colon + 1;
                for (pos i = b; i < trimmed; ++i) {
                    if (src_[i] != ':')
                        continue;
                    c_.t.heading_tags.names.values.push_back(
                        c_.intern(src_.view(b, i)));
                    b = i + 1;
                }
            }
        }
        const pos tb = src_.skip_blank(title_start, title_end);
        const pos te = src_.skip_blank_back(title_end, tb);
        t.titles[h] = {tb, te};
        parse(tb, te, title_set);
    }

    context& c_;
    const text& src_;
};

}

void parse_objects(context& c) {
    object_parser(c).run();
}

}

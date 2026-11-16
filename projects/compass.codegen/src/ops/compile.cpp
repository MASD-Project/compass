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
#include "compass.codegen/ops/compile.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace compass::codegen::ops {

namespace {

using types::node_id;
using types::node_kind;

/**
 * @brief Python's \s for str patterns, over ASCII.
 */
bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
        c == '\v' || (c >= '\x1c' && c <= '\x1f');
}

bool is_line_end(char c) noexcept { return c == '\r' || c == '\n'; }

/**
 * @brief One tag as pystache's tag expression matches it.
 *
 * start is where the match starts, at the spaces and tabs before the
 * opening delimiter; end is past the closing one. type is the tag's sigil,
 * '=' for a delimiter change and '&' for a triple mustache, or 0.
 */
struct tag {
    std::size_t start = 0;
    std::size_t open = 0;
    std::size_t end = 0;
    char type = 0;
    std::size_t key_first = 0;
    std::size_t key_size = 0;
};

class scanner final {
public:
    explicit scanner(std::string_view s) : s_(s) {}

    void set_delimiters(std::string open, std::string close) {
        open_ = std::move(open);
        close_ = std::move(close);
    }

    /**
     * @brief The first tag that starts at or after @p from.
     *
     * The match starts at the first opening delimiter for which the rest of
     * the expression matches, taken back over the spaces and tabs before it.
     */
    std::optional<tag> find(std::size_t from) const {
        for (auto o = s_.find(open_, from); o != std::string_view::npos;
             o = s_.find(open_, o + 1)) {
            auto m = match(o + open_.size());
            if (!m)
                continue;
            auto start = o;
            while (start > from && (s_[start - 1] == ' ' ||
                s_[start - 1] == '\t'))
                --start;
            m->start = start;
            m->open = o;
            return m;
        }
        return std::nullopt;
    }

private:
    std::size_t skip_space(std::size_t p) const noexcept {
        while (p < s_.size() && is_space(s_[p]))
            ++p;
        return p;
    }

    bool closes_at(std::size_t p) const noexcept {
        return s_.substr(p, close_.size()) == close_;
    }

    /**
     * @brief =delims= or {name}: a key of at least one character other than
     * a line end, as short as it can be, then @p terminator and the closing
     * delimiter, with any whitespace around them.
     */
    std::optional<tag> bracketed(std::size_t q, char type,
        char terminator) const {
        for (auto r = q + 1; r <= s_.size() && s_[r - 1] != '\n'; ++r) {
            const auto t = skip_space(r);
            if (t >= s_.size() || s_[t] != terminator)
                continue;
            const auto c = skip_space(t + 1);
            if (closes_at(c))
                return tag{0, 0, c + close_.size(), type, q, r - q};
        }
        return std::nullopt;
    }

    /**
     * @brief A key of at least one character of any kind, as short as it
     * can be, then whitespace and the closing delimiter.
     */
    std::optional<tag> plain(std::size_t q, char type) const {
        const auto c = s_.find(close_, q + 1);
        if (c == std::string_view::npos)
            return std::nullopt;
        auto r = c;
        while (r > q + 1 && is_space(s_[r - 1]))
            --r;
        return tag{0, 0, c + close_.size(), type, q, r - q};
    }

    /**
     * @brief The expression after the opening delimiter at @p p. Each \s*
     * is greedy and gives back a character at a time when what follows it
     * fails, so the loops try the longest run first.
     */
    std::optional<tag> match(std::size_t p0) const {
        const auto spaces = skip_space(p0) - p0;
        for (std::size_t w = spaces + 1; w-- > 0;) {
            const auto p = p0 + w;
            if (p >= s_.size())
                continue;
            const char c = s_[p];
            if (c == '=' || c == '{') {
                const auto q0 = p + 1;
                const auto inner = skip_space(q0) - q0;
                for (std::size_t v = inner + 1; v-- > 0;) {
                    if (auto m = bracketed(q0 + v, c == '=' ? '=' : '&',
                            c == '=' ? '=' : '}'))
                        return m;
                }
            }
            const bool sigil = c == '!' || c == '>' || c == '&' ||
                c == '/' || c == '#' || c == '^';
            for (int with = sigil ? 1 : 0; with >= 0; --with) {
                const auto q0 = p + static_cast<std::size_t>(with);
                const auto inner = skip_space(q0) - q0;
                for (std::size_t v = inner + 1; v-- > 0;) {
                    if (auto m = plain(q0 + v, with ? c : 0))
                        return m;
                }
            }
        }
        return std::nullopt;
    }

    std::string_view s_;
    std::string open_ = "{{";
    std::string close_ = "}}";
};

std::vector<std::string> split_whitespace(std::string_view s) {
    std::vector<std::string> parts;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && is_space(s[i]))
            ++i;
        const auto first = i;
        while (i < s.size() && !is_space(s[i]))
            ++i;
        if (i > first)
            parts.emplace_back(s.substr(first, i - first));
    }
    return parts;
}

class compiler final {
public:
    explicit compiler(types::compiled_template& t) : t_(t) {}

    node_id add(node_kind kind, std::size_t first, std::size_t size) {
        const node_id id{static_cast<std::uint32_t>(t_.size())};
        t_.kinds.values.push_back(kind);
        t_.firsts.values.push_back(static_cast<std::uint32_t>(first));
        t_.sizes.values.push_back(static_cast<std::uint32_t>(size));
        t_.ends.values.push_back(id.next());
        t_.indent_firsts.values.push_back(0);
        t_.indent_sizes.values.push_back(0);
        return id;
    }

    void run() {
        const std::string_view s = t_.source;
        scanner scan(s);
        std::vector<node_id> open;
        std::size_t start = 0;
        while (auto m = scan.find(start)) {
            auto match_index = m->start;
            auto end = m->end;
            const char type = m->type;
            const auto key = s.substr(m->key_first, m->key_size);
            auto indent_first = m->start;
            auto indent_size = m->open - m->start;
            const bool begins_line = match_index == 0 ||
                is_line_end(s[match_index - 1]);
            const bool ends_line = end == s.size() || is_line_end(s[end]);
            const bool interpolates = type == 0 || type == '&';
            if (begins_line && ends_line && !interpolates) {
                if (end < s.size() && s[end] == '\r')
                    ++end;
                if (end < s.size() && s[end] == '\n')
                    ++end;
            } else if (indent_size != 0) {
                match_index += indent_size;
                indent_size = 0;
            }
            if (start != match_index)
                add(node_kind::text, start, match_index - start);
            start = end;

            switch (type) {
            case '#':
            case '^':
                open.push_back(add(type == '#' ? node_kind::section
                    : node_kind::inverted, m->key_first, m->key_size));
                break;
            case '/': {
                if (open.empty() || t_.text(open.back()) != key)
                    throw template_error("section end tag mismatch at " +
                        std::to_string(m->open) + ": {{/" + std::string(key) +
                        "}}");
                t_.ends[open.back()] = node_id{
                    static_cast<std::uint32_t>(t_.size())};
                open.pop_back();
                break;
            }
            case '!':
                break;
            case '=': {
                auto parts = split_whitespace(key);
                if (parts.size() < 2)
                    throw template_error("bad delimiter change at " +
                        std::to_string(m->open));
                scan.set_delimiters(std::move(parts[0]), std::move(parts[1]));
                break;
            }
            case '>': {
                const auto n = add(node_kind::partial, m->key_first,
                    m->key_size);
                t_.indent_firsts[n] = static_cast<std::uint32_t>(indent_first);
                t_.indent_sizes[n] = static_cast<std::uint32_t>(indent_size);
                break;
            }
            case '&':
                add(node_kind::literal, m->key_first, m->key_size);
                break;
            default:
                add(node_kind::escaped, m->key_first, m->key_size);
                break;
            }
        }
        if (!open.empty())
            throw template_error("section {{#" +
                std::string(t_.text(open.back())) + "}} is not closed");
        if (start != s.size())
            add(node_kind::text, start, s.size() - start);
    }

private:
    types::compiled_template& t_;
};

}

types::compiled_template compile(std::string source) {
    types::compiled_template t;
    t.source = std::move(source);
    compiler(t).run();
    return t;
}

}

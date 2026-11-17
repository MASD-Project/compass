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
#include "compass.cpp/ops/lexer.hpp"

#include <algorithm>

namespace compass::cpp::ops {

namespace {

constexpr std::size_t npos = std::string_view::npos;

constexpr bool is_name(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '$' ||
        static_cast<unsigned char>(c) >= 0x80;
}

constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

constexpr bool is_blank(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

bool is_encoding_prefix(std::string_view p) noexcept {
    return p == "L" || p == "u" || p == "U" || p == "u8";
}

class lexer {
public:
    explicit lexer(std::string_view source) : s_(source), n_(source.size()) {
        l_.code.assign(source);
    }

    lexed run() {
        bool line_start = true;
        std::size_t directive = npos;
        for (std::size_t i = 0; i < n_;) {
            const char c = s_[i];
            if (const auto after = splice_end(i); after != npos) {
                i = after;
                continue;
            }
            if (c == '\n') {
                if (directive != npos)
                    l_.directives.push_back(span_of(directive, i));
                directive = npos;
                line_start = true;
                ++i;
            } else if (is_blank(c)) {
                ++i;
            } else if (c == '/' && i + 1 < n_ && s_[i + 1] == '/') {
                i = line_comment(i);
            } else if (c == '/' && i + 1 < n_ && s_[i + 1] == '*') {
                auto end = s_.find("*/", i + 2);
                end = end == npos ? n_ : end + 2;
                blank(i, end);
                i = end;
            } else if (c == '#' && line_start) {
                directive = i;
                line_start = false;
                ++i;
            } else {
                line_start = false;
                i = token(i);
            }
        }
        if (directive != npos)
            l_.directives.push_back(span_of(directive, n_));
        mark_dead();
        return std::move(l_);
    }

private:
    static span span_of(std::size_t b, std::size_t e) noexcept {
        return {static_cast<std::uint32_t>(b), static_cast<std::uint32_t>(e)};
    }

    void blank(std::size_t b, std::size_t e) noexcept {
        for (auto k = b; k < e; ++k) {
            if (s_[k] != '\n')
                l_.code[k] = ' ';
        }
    }

    /**
     * @brief Past the splice at @p i, recorded and its backslash blanked,
     * or npos when none starts there.
     */
    std::size_t splice_end(std::size_t i) {
        if (s_[i] != '\\')
            return npos;
        auto nl = i + 1;
        if (nl + 1 < n_ && s_[nl] == '\r' && s_[nl + 1] == '\n')
            ++nl;
        if (nl >= n_ || s_[nl] != '\n')
            return npos;
        blank(i, nl);
        l_.splices.push_back(static_cast<std::uint32_t>(nl));
        return nl + 1;
    }

    std::size_t line_comment(std::size_t i) {
        auto j = i + 2;
        while (j < n_ && s_[j] != '\n') {
            if (const auto after = splice_end(j); after != npos)
                j = after;
            else
                ++j;
        }
        blank(i, j);
        return j;
    }

    void literal(std::size_t b, std::size_t e) {
        blank(b, e);
        l_.literals.push_back(span_of(b, e));
    }

    /**
     * @brief Past the string whose opening quote is at @p quote: its
     * closing quote, or the end of its line when it has none.
     */
    std::size_t string_end(std::size_t quote, char close) {
        auto j = quote + 1;
        while (j < n_) {
            if (const auto after = splice_end(j); after != npos) {
                j = after;
            } else if (s_[j] == '\\') {
                j += 2;
            } else if (s_[j] == close) {
                return j + 1;
            } else if (s_[j] == '\n') {
                return npos;
            } else {
                ++j;
            }
        }
        return npos;
    }

    /**
     * @brief Past the raw string whose quote is at @p quote, or npos when
     * no valid delimiter and parenthesis follow it.
     */
    std::size_t raw_string_end(std::size_t quote) const {
        auto paren = quote + 1;
        while (paren < n_ && paren - quote <= 17 && s_[paren] != '(') {
            const char c = s_[paren];
            if (c == ' ' || c == ')' || c == '\\' || c == '"' ||
                c == '\n' || c == '\t')
                return npos;
            ++paren;
        }
        if (paren >= n_ || s_[paren] != '(')
            return npos;
        std::string closer(")");
        closer.append(s_.substr(quote + 1, paren - quote - 1));
        closer.push_back('"');
        const auto stop = s_.find(closer, paren + 1);
        return stop == npos ? n_ : stop + closer.size();
    }

    std::size_t number_end(std::size_t i) const noexcept {
        auto j = i + 1;
        while (j < n_) {
            const char c = s_[j];
            if ((c == '+' || c == '-') && (s_[j - 1] == 'e' ||
                    s_[j - 1] == 'E' || s_[j - 1] == 'p' ||
                    s_[j - 1] == 'P'))
                ++j;
            else if (c == '\'' && j + 1 < n_ && is_name(s_[j + 1]))
                j += 2;
            else if (is_name(c) || c == '.')
                ++j;
            else
                break;
        }
        return j;
    }

    std::size_t token(std::size_t i) {
        const char c = s_[i];
        if (is_digit(c) || (c == '.' && i + 1 < n_ && is_digit(s_[i + 1])))
            return number_end(i);
        if (is_name(c)) {
            auto end = i;
            while (end < n_ && is_name(s_[end]))
                ++end;
            const auto word = s_.substr(i, end - i);
            if (end < n_ && s_[end] == '"' && word.ends_with('R') &&
                (word.size() == 1 ||
                    is_encoding_prefix(word.substr(0, word.size() - 1)))) {
                if (const auto stop = raw_string_end(end); stop != npos) {
                    literal(i, stop);
                    return stop;
                }
            }
            if (end < n_ && (s_[end] == '"' || s_[end] == '\'') &&
                is_encoding_prefix(word))
                return quoted(i, end);
            return end;
        }
        if (c == '"' || c == '\'')
            return quoted(i, i);
        return i + 1;
    }

    /**
     * @brief A string or character literal starting at @p b, its quote at
     * @p quote. A character quote that nothing closes on its line opens
     * nothing.
     */
    std::size_t quoted(std::size_t b, std::size_t quote) {
        const auto end = string_end(quote, s_[quote]);
        if (end != npos) {
            literal(b, end);
            return end;
        }
        if (s_[quote] == '\'')
            return quote + 1;
        auto stop = s_.find('\n', quote);
        stop = stop == npos ? n_ : stop;
        literal(b, stop);
        return stop;
    }

    struct conditional {
        bool outer_live = true;
        bool taken = false;
    };

    void mark_dead() {
        std::vector<conditional> open;
        bool live = true;
        for (std::size_t k = 0; k < l_.directives.size(); ++k) {
            const auto d = l_.directives[k];
            const auto p = parts_of(l_, d);
            auto condition = p.rest;
            while (!condition.empty() && (is_blank(condition.front()) ||
                       condition.front() == '\n'))
                condition.remove_prefix(1);
            while (!condition.empty() && (is_blank(condition.back()) ||
                       condition.back() == '\n'))
                condition.remove_suffix(1);
            const auto& w = p.keyword;
            if (w == "if" || w == "ifdef" || w == "ifndef") {
                const auto known = w == "if" ? condition : "";
                open.push_back({live, known == "1"});
                live = live && known != "0";
            } else if ((w == "elif" || w == "else") && !open.empty()) {
                auto& c = open.back();
                const auto known = w == "elif" ? condition : "1";
                live = c.outer_live && !c.taken && known != "0";
                c.taken = c.taken || known == "1";
            } else if (w == "endif" && !open.empty()) {
                live = open.back().outer_live;
                open.pop_back();
            }
            if (live)
                continue;
            const auto next = k + 1 < l_.directives.size()
                ? l_.directives[k + 1].end : static_cast<std::uint32_t>(n_);
            if (next > d.end)
                l_.dead.push_back({d.end, next});
        }
    }

    std::string_view s_;
    std::size_t n_;
    lexed l_;
};

}

bool lexed::is_dead(std::uint32_t offset) const noexcept {
    const auto it = std::ranges::upper_bound(dead, offset, {},
        &span::begin);
    return it != dead.begin() && offset < std::prev(it)->end;
}

lexed lex(std::string_view source) {
    return lexer(source).run();
}

directive_parts parts_of(const lexed& l, span d) noexcept {
    const std::string_view code(l.code);
    auto i = static_cast<std::size_t>(d.begin) + 1;
    while (i < d.end && (is_blank(code[i]) || code[i] == '\n'))
        ++i;
    auto end = i;
    while (end < d.end && is_name(code[end]))
        ++end;
    return {code.substr(i, end - i), code.substr(end, d.end - end),
        static_cast<std::uint32_t>(end)};
}

}

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
#include "compass.bench/ops/include_scan.hpp"

#include <algorithm>
#include <array>

namespace compass::bench::ops {

namespace {

bool is_blank(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

bool is_identifier(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_';
}

bool is_digit(char c) { return c >= '0' && c <= '9'; }

/**
 * @brief Walks the bytes of one file, tracking the line number.
 */
class scanner final {
public:
    scanner(std::string_view text, std::uint32_t includer,
        types::include_table& out)
        : begin_(text.data()), p_(text.data()),
          end_(text.data() + text.size()), includer_(includer), out_(out) {}

    void run() {
        bool line_start = true;
        while (p_ < end_) {
            const char c = *p_;
            if (c == '\n') {
                ++line_;
                line_start = true;
                ++p_;
            } else if (is_blank(c)) {
                ++p_;
            } else if (c == '/' && peek(1) == '/') {
                skip_line_comment();
            } else if (c == '/' && peek(1) == '*') {
                skip_block_comment();
            } else if (c == '#' && line_start) {
                ++p_;
                directive();
                line_start = false;
            } else if (c == '"') {
                line_start = false;
                if (is_raw_prefix())
                    skip_raw_string();
                else
                    skip_literal('"');
            } else if (c == '\'' && !is_digit_separator()) {
                line_start = false;
                skip_literal('\'');
            } else {
                line_start = false;
                ++p_;
            }
        }
    }

private:
    char peek(std::ptrdiff_t ahead) const {
        return end_ - p_ > ahead ? p_[ahead] : '\0';
    }

    /**
     * @brief The identifier or number that ends just before p_.
     */
    std::string_view token_before() const {
        const char* start = p_;
        while (start > begin_ && is_identifier(start[-1]))
            --start;
        return {start, static_cast<std::size_t>(p_ - start)};
    }

    bool is_raw_prefix() const {
        const auto t = token_before();
        return t == "R" || t == "u8R" || t == "uR" || t == "UR" || t == "LR";
    }

    bool is_digit_separator() const {
        const auto t = token_before();
        return !t.empty() && is_digit(t.front());
    }

    void skip_line_comment() {
        p_ += 2;
        while (p_ < end_ && *p_ != '\n') {
            if (*p_ == '\\' && peek(1) == '\n') {
                ++line_;
                ++p_;
            } else if (*p_ == '\\' && peek(1) == '\r' && peek(2) == '\n') {
                ++line_;
                p_ += 2;
            }
            ++p_;
        }
    }

    void skip_block_comment() {
        p_ += 2;
        while (p_ < end_) {
            if (*p_ == '*' && peek(1) == '/') {
                p_ += 2;
                return;
            }
            if (*p_ == '\n')
                ++line_;
            ++p_;
        }
    }

    /**
     * @brief Skips a string or char literal; stops at an unescaped newline.
     */
    void skip_literal(char quote) {
        ++p_;
        while (p_ < end_) {
            const char c = *p_;
            if (c == quote) {
                ++p_;
                return;
            }
            if (c == '\n')
                return;
            if (c == '\\' && p_ + 1 < end_) {
                if (p_[1] == '\n')
                    ++line_;
                ++p_;
            }
            ++p_;
        }
    }

    void skip_raw_string() {
        ++p_;
        const char* open = p_;
        while (p_ < end_ && *p_ != '(' && *p_ != '\n' && p_ - open <= 16)
            ++p_;
        if (p_ >= end_ || *p_ != '(')
            return;
        const std::string_view delimiter(open,
            static_cast<std::size_t>(p_ - open));
        ++p_;
        for (; p_ < end_; ++p_) {
            if (*p_ == '\n') {
                ++line_;
            } else if (*p_ == ')' &&
                static_cast<std::size_t>(end_ - p_) > delimiter.size() + 1 &&
                std::string_view(p_ + 1, delimiter.size()) == delimiter &&
                p_[1 + delimiter.size()] == '"') {
                p_ += delimiter.size() + 2;
                return;
            }
        }
    }

    void skip_blanks() {
        while (p_ < end_ && is_blank(*p_))
            ++p_;
    }

    void directive() {
        skip_blanks();
        constexpr std::string_view word = "include";
        if (static_cast<std::size_t>(end_ - p_) < word.size() ||
            std::string_view(p_, word.size()) != word)
            return;
        if (end_ - p_ > static_cast<std::ptrdiff_t>(word.size()) &&
            is_identifier(p_[word.size()]))
            return;
        p_ += word.size();
        skip_blanks();
        if (p_ >= end_)
            return;

        types::include_kind kind;
        char close;
        if (*p_ == '"') {
            kind = types::include_kind::quoted;
            close = '"';
        } else if (*p_ == '<') {
            kind = types::include_kind::angle;
            close = '>';
        } else {
            ++out_.computed;
            return;
        }
        const char* name = ++p_;
        while (p_ < end_ && *p_ != close && *p_ != '\n')
            ++p_;
        if (p_ >= end_ || *p_ != close)
            return;
        append(kind, std::string_view(name,
            static_cast<std::size_t>(p_ - name)));
        ++p_;
    }

    void append(types::include_kind kind, std::string_view name) {
        out_.includers.push_back(includer_);
        out_.lines.push_back(line_);
        out_.kinds.push_back(kind);
        out_.name_offsets.push_back(
            static_cast<std::uint32_t>(out_.names.size()));
        out_.name_sizes.push_back(static_cast<std::uint32_t>(name.size()));
        out_.names.append(name);
    }

    const char* begin_;
    const char* p_;
    const char* end_;
    std::uint32_t includer_;
    std::uint32_t line_ = 1;
    types::include_table& out_;
};

}

bool is_scanned_extension(std::string_view extension) {
    constexpr std::array<std::string_view, 8> scanned = {".hpp", ".cpp", ".h",
        ".hh", ".cc", ".cxx", ".ipp", ".tpp"};
    return std::ranges::find(scanned, extension) != scanned.end();
}

void scan_includes(std::string_view text, std::uint32_t includer,
    types::include_table& out) {
    scanner(text, includer, out).run();
}

}

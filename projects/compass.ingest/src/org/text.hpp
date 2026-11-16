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
#ifndef COMPASS_INGEST_ORG_TEXT_HPP
#define COMPASS_INGEST_ORG_TEXT_HPP

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace compass::ingest::org::detail {

using pos = std::uint32_t;

inline constexpr pos npos = 0xFFFF'FFFF;

/**
 * @brief Syntax class of each ASCII character in org-mode's syntax table,
 * as Emacs prints it: w word, _ symbol, . punctuation, space, ( ) and ".
 */
inline constexpr std::string_view ascii_syntax =
    ".........  .  .................. .\".ww_w()__._._wwwwwwwwww..(_).."
    "wwwwwwwwwwwwwwwwwwwwwwwwww(_)._.wwwwwwwwwwwwwwwwwwwwwwwwww(_)_.";

static_assert(ascii_syntax.size() == 128);

[[nodiscard]] constexpr bool is_ascii(char c) noexcept {
    return static_cast<unsigned char>(c) < 0x80;
}

[[nodiscard]] constexpr bool is_blank(char c) noexcept {
    return c == ' ' || c == '\t';
}

/**
 * @brief The characters org-element skips between elements.
 */
[[nodiscard]] constexpr bool is_ws(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

[[nodiscard]] constexpr bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

[[nodiscard]] constexpr bool is_ascii_alpha(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

[[nodiscard]] constexpr bool is_ascii_alnum(char c) noexcept {
    return is_ascii_alpha(c) || is_digit(c);
}

[[nodiscard]] constexpr char to_lower(char c) noexcept {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] constexpr char to_upper(char c) noexcept {
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}

[[nodiscard]] constexpr char syntax_of(char c) noexcept {
    return is_ascii(c) ? ascii_syntax[static_cast<unsigned char>(c)] : 'w';
}

/**
 * @brief Whether a code point above ASCII has whitespace syntax in Emacs.
 */
[[nodiscard]] constexpr bool is_wide_space(char32_t c) noexcept {
    return c == 0xA0 || (c >= 0x2000 && c <= 0x200B) || c == 0x202F ||
        c == 0x205F || c == 0x3000;
}

/**
 * @brief Whether a code point above ASCII is a letter or digit to Emacs.
 *
 * An approximation of the Unicode general categories: everything from
 * U+00C0 up is a letter, except the blocks of punctuation, symbols, arrows,
 * box drawing, dingbats and emoji.
 */
[[nodiscard]] constexpr bool is_wide_alnum(char32_t c) noexcept {
    if (c < 0xAA)
        return false;
    if (c < 0xC0)
        return c == 0xAA || c == 0xB5 || c == 0xBA;
    if (c == 0xD7 || c == 0xF7)
        return false;
    if (c >= 0x2000 && c <= 0x2BFF)
        return false;
    if (c >= 0x3000 && c <= 0x303F)
        return false;
    if (c >= 0xE000 && c <= 0xF8FF)
        return false;
    if (c >= 0xFE00 && c <= 0xFE0F)
        return false;
    return c < 0x1F000 || c > 0x1FAFF;
}

/**
 * @brief Whether a code point above ASCII has word syntax in Emacs.
 *
 * Exact for Latin-1 and for the punctuation, arrow, mathematical, box
 * drawing and CJK punctuation blocks; everything else counts as a word
 * character, which leaves out a few symbols scattered through other blocks.
 */
[[nodiscard]] constexpr bool is_wide_word(char32_t c) noexcept {
    if (c < 0xC0)
        return c == 0xA5 || c == 0xB2 || c == 0xB3 || c == 0xB5 ||
            c == 0xB7 || c == 0xB9 || c < 0xA0;
    if (c == 0xD7 || c == 0xF7)
        return false;
    if (c < 0x2000)
        return true;
    if (c <= 0x2026 || (c >= 0x202F && c <= 0x205F) || c == 0x20AC)
        return false;
    if ((c >= 0x2190 && c <= 0x244F) || (c >= 0x2500 && c <= 0x254B))
        return false;
    if ((c >= 0x2A00 && c <= 0x2BFF) || (c >= 0x2E00 && c <= 0x2E7F))
        return false;
    return !((c >= 0x3000 && c <= 0x3003) || (c >= 0x3008 && c <= 0x301C));
}

/**
 * @brief The bytes of one document, with the line and character primitives
 * the parser is written in.
 *
 * A line ends at a line feed. Whether a carriage return before it belongs
 * to the line end is decided as Emacs decides it: when every line feed of
 * the file follows a carriage return the file has CRLF line ends and parses
 * like one with LF; one bare line feed makes every carriage return a
 * character of its line. A file with carriage returns alone is one line.
 */
class text final {
public:
    explicit text(std::string_view source) noexcept
        : s_(source.data()), n_(static_cast<pos>(source.size())),
          crlf_(every_line_ends_with_crlf()) {}

    [[nodiscard]] pos size() const noexcept { return n_; }

    [[nodiscard]] char operator[](pos p) const noexcept { return s_[p]; }

    /**
     * @brief The character at p, or a line feed at and past the end.
     */
    [[nodiscard]] char at(pos p) const noexcept {
        return p < n_ ? s_[p] : '\n';
    }

    [[nodiscard]] std::string_view view(pos b, pos e) const noexcept {
        return {s_ + b, e - b};
    }

    [[nodiscard]] bool at_bol(pos p) const noexcept {
        return p == 0 || s_[p - 1] == '\n';
    }

    /**
     * @brief Where the text of the line holding p ends.
     */
    [[nodiscard]] pos eol(pos p) const noexcept {
        const pos q = line_feed(p);
        return crlf_ && q > p && s_[q - 1] == '\r' ? q - 1 : q;
    }

    /**
     * @brief Whether p is at the end of the text of its line.
     */
    [[nodiscard]] bool at_eol(pos p) const noexcept {
        return p >= n_ || s_[p] == '\n' ||
            (crlf_ && s_[p] == '\r' && p + 1 < n_ && s_[p + 1] == '\n');
    }

    /**
     * @brief The start of the line after the one holding p, or the end.
     */
    [[nodiscard]] pos next_line(pos p) const noexcept {
        const pos q = line_feed(p);
        return q < n_ ? q + 1 : n_;
    }

    [[nodiscard]] pos bol(pos p) const noexcept {
        while (p > 0 && s_[p - 1] != '\n')
            --p;
        return p;
    }

    [[nodiscard]] pos skip_blank(pos p) const noexcept {
        while (p < n_ && is_blank(s_[p]))
            ++p;
        return p;
    }

    [[nodiscard]] pos skip_blank(pos p, pos limit) const noexcept {
        while (p < limit && is_blank(s_[p]))
            ++p;
        return p;
    }

    [[nodiscard]] pos skip_blank_back(pos p, pos floor) const noexcept {
        while (p > floor && is_blank(s_[p - 1]))
            --p;
        return p;
    }

    [[nodiscard]] pos skip_ws(pos p, pos limit) const noexcept {
        while (p < limit && is_ws(s_[p]))
            ++p;
        return p;
    }

    [[nodiscard]] pos skip_ws_back(pos p, pos floor) const noexcept {
        while (p > floor && is_ws(s_[p - 1]))
            --p;
        return p;
    }

    /**
     * @brief Whether the line starting at p holds blanks only.
     */
    [[nodiscard]] bool blank_line(pos p) const noexcept {
        return at_eol(skip_blank(p));
    }

    /**
     * @brief Emacs count-lines: the line feeds in [a, b), plus one when the
     * range ends inside a line.
     */
    [[nodiscard]] pos count_lines(pos a, pos b) const noexcept {
        if (a >= b)
            return 0;
        const auto feeds =
            static_cast<pos>(std::count(s_ + a, s_ + b, '\n'));
        return s_[b - 1] == '\n' ? feeds : feeds + 1;
    }

    /**
     * @brief Where an element that stops at p ends: the start of the next
     * line that is not blank, or limit.
     */
    [[nodiscard]] pos after_blank_lines(pos p, pos limit) const noexcept {
        const pos q = skip_ws(p, limit);
        return q >= n_ ? n_ : std::max(p, bol(q));
    }

    /**
     * @brief Whether the bytes at p equal literal, ignoring ASCII case.
     *
     * literal is given in lower case.
     */
    [[nodiscard]] bool match_ci(pos p, std::string_view literal) const
        noexcept {
        if (n_ - p < literal.size())
            return false;
        for (std::size_t i = 0; i < literal.size(); ++i) {
            if (to_lower(s_[p + i]) != literal[i])
                return false;
        }
        return true;
    }

    [[nodiscard]] bool match(pos p, std::string_view literal) const noexcept {
        return n_ - p >= literal.size() &&
            std::memcmp(s_ + p, literal.data(), literal.size()) == 0;
    }

    /**
     * @brief First position of c in [p, limit), or npos.
     */
    [[nodiscard]] pos find(pos p, pos limit, char c) const noexcept {
        if (p >= limit)
            return npos;
        const void* f = std::memchr(s_ + p, c, limit - p);
        return f ? static_cast<pos>(static_cast<const char*>(f) - s_) : npos;
    }

    /**
     * @brief First position of literal in [p, limit), or npos.
     */
    [[nodiscard]] pos find(pos p, pos limit, std::string_view literal) const
        noexcept {
        if (p >= limit)
            return npos;
        const auto f = view(p, limit).find(literal);
        return f == std::string_view::npos ? npos
                                           : p + static_cast<pos>(f);
    }

    /**
     * @brief The code point that starts at p; a malformed byte is itself.
     */
    [[nodiscard]] char32_t code_point(pos p) const noexcept {
        const auto b = static_cast<unsigned char>(s_[p]);
        if (b < 0xC0)
            return b;
        const int extra = b >= 0xF0 ? 3 : b >= 0xE0 ? 2 : 1;
        if (p + extra >= n_)
            return b;
        char32_t c = b & (0x3F >> extra);
        for (int i = 1; i <= extra; ++i)
            c = (c << 6) | (static_cast<unsigned char>(s_[p + i]) & 0x3F);
        return c;
    }

    /**
     * @brief The start of the code point that ends at p.
     */
    [[nodiscard]] pos previous(pos p) const noexcept {
        pos q = p - 1;
        while (q > 0 && p - q < 4 &&
            (static_cast<unsigned char>(s_[q]) & 0xC0) == 0x80)
            --q;
        return q;
    }

    /**
     * @brief Bytes of the code point that starts at p.
     */
    [[nodiscard]] pos width(pos p) const noexcept {
        const auto b = static_cast<unsigned char>(s_[p]);
        const pos w = b < 0xC0 ? 1 : b < 0xE0 ? 2 : b < 0xF0 ? 3 : 4;
        return std::min(w, n_ - p);
    }

    /**
     * @brief Emacs [:space:]: whitespace syntax.
     */
    [[nodiscard]] bool space_at(pos p) const noexcept {
        const char c = s_[p];
        if (is_ascii(c))
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                c == '\f';
        return is_wide_space(code_point(p));
    }

    [[nodiscard]] bool alnum_at(pos p) const noexcept {
        const char c = s_[p];
        return is_ascii(c) ? is_ascii_alnum(c) : is_wide_alnum(code_point(p));
    }

    [[nodiscard]] bool alpha_at(pos p) const noexcept {
        const char c = s_[p];
        return is_ascii(c) ? is_ascii_alpha(c) : is_wide_alnum(code_point(p));
    }

    /**
     * @brief Emacs [:word:]: word syntax, which in org-mode takes in $, %
     * and the apostrophe.
     */
    [[nodiscard]] bool word_at(pos p) const noexcept {
        const char c = s_[p];
        return is_ascii(c) ? syntax_of(c) == 'w'
                           : is_wide_word(code_point(p));
    }

    /**
     * @brief Emacs [:punct:]: ASCII punctuation, and above ASCII anything
     * without word syntax.
     */
    [[nodiscard]] bool punct_at(pos p) const noexcept {
        const char c = s_[p];
        if (is_ascii(c))
            return c > ' ' && c != 0x7F && !is_ascii_alnum(c);
        return !is_wide_word(code_point(p));
    }

private:
    [[nodiscard]] pos line_feed(pos p) const noexcept {
        if (p >= n_)
            return n_;
        const void* f = std::memchr(s_ + p, '\n', n_ - p);
        return f ? static_cast<pos>(static_cast<const char*>(f) - s_) : n_;
    }

    [[nodiscard]] bool every_line_ends_with_crlf() const noexcept {
        pos p = 0;
        for (;;) {
            const pos q = line_feed(p);
            if (q >= n_)
                return p > 0;
            if (q == 0 || s_[q - 1] != '\r')
                return false;
            p = q + 1;
        }
    }

    const char* s_;
    pos n_;
    bool crlf_;
};

}

#endif

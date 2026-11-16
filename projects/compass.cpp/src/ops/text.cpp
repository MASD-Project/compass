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
#include "compass.cpp/ops/text.hpp"

#include <algorithm>

namespace compass::cpp::ops {

namespace {

constexpr char fold(char c) noexcept {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

/**
 * @brief How many bytes the line separator at @p i is, 0 when there is
 * none.
 */
std::size_t separator_at(std::string_view text, std::size_t i) noexcept {
    const auto c = text[i];
    if (c == '\r')
        return i + 1 < text.size() && text[i + 1] == '\n' ? 2 : 1;
    if (c == '\n' || c == '\v' || c == '\f' || c == '\x1c' || c == '\x1d' ||
        c == '\x1e')
        return 1;
    if (text.substr(i, 2) == "\xc2\x85")
        return 2;
    if (text.substr(i, 3) == "\xe2\x80\xa8" ||
        text.substr(i, 3) == "\xe2\x80\xa9")
        return 3;
    return 0;
}

}

std::size_t skip_spaces(std::string_view text, std::size_t i) noexcept {
    while (i < text.size() && is_space(text[i]))
        ++i;
    return i;
}

bool matches_folded(std::string_view text, std::size_t i,
    std::string_view word) noexcept {
    if (i + word.size() > text.size())
        return false;
    for (std::size_t k = 0; k < word.size(); ++k) {
        if (fold(text[i + k]) != fold(word[k]))
            return false;
    }
    return true;
}

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size();) {
        if (const auto n = separator_at(text, i); n != 0) {
            out.push_back(text.substr(start, i - start));
            i += n;
            start = i;
        } else {
            ++i;
        }
    }
    if (start < text.size())
        out.push_back(text.substr(start));
    return out;
}

std::string_view strip(std::string_view text) noexcept {
    std::size_t begin = 0;
    auto end = text.size();
    while (begin < end && is_space(text[begin]))
        ++begin;
    while (end > begin && is_space(text[end - 1]))
        --end;
    return text.substr(begin, end - begin);
}

std::uint32_t line_of(std::string_view text, std::size_t offset) noexcept {
    const auto before = text.substr(0, std::min(offset, text.size()));
    return static_cast<std::uint32_t>(1 + std::ranges::count(before, '\n'));
}

std::uint32_t offset_of_line(std::string_view text,
    std::uint32_t line) noexcept {
    std::size_t at = 0;
    for (std::uint32_t l = 1; l < line; ++l) {
        const auto newline = text.find('\n', at);
        if (newline == std::string_view::npos)
            return static_cast<std::uint32_t>(text.size());
        at = newline + 1;
    }
    return static_cast<std::uint32_t>(at);
}

std::size_t characters(std::string_view text) noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(text, [](char c) {
        return (static_cast<unsigned char>(c) & 0xC0) != 0x80;
    }));
}

}

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
#include "compass.crosscheck/ops/words.hpp"

#include "compass.cpp/ops/text.hpp"

namespace compass::crosscheck::ops {

std::string lowered(std::string_view text) {
    std::string out(text);
    for (auto& c : out) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> out;
    for (std::size_t start = 0;;) {
        const auto at = text.find(separator, start);
        out.push_back(text.substr(start, at == std::string_view::npos
                ? std::string_view::npos : at - start));
        if (at == std::string_view::npos)
            return out;
        start = at + 1;
    }
}

std::string collapse_spaces(std::string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        if (compass::cpp::ops::is_space(text[i])) {
            out.push_back(' ');
            i = compass::cpp::ops::skip_spaces(text, i);
        } else {
            out.push_back(text[i++]);
        }
    }
    return out;
}

namespace {

/**
 * @brief The length of the space character at @p i of UTF-8 @p text, as
 * str.isspace() decides, or zero when it is not a space.
 */
std::size_t space_at(std::string_view text, std::size_t i) {
    const auto c = static_cast<unsigned char>(text[i]);
    if (c < 0x80)
        return compass::cpp::ops::is_space(text[i]) ? 1 : 0;
    const auto byte = [&](std::size_t k) {
        return i + k < text.size()
            ? static_cast<unsigned char>(text[i + k]) : 0;
    };
    if (c == 0xC2 && (byte(1) == 0x85 || byte(1) == 0xA0))
        return 2;
    if (c == 0xE1 && byte(1) == 0x9A && byte(2) == 0x80)
        return 3;
    if (c == 0xE2 && byte(1) == 0x80 &&
        (byte(2) <= 0x8A || byte(2) == 0xA8 || byte(2) == 0xA9 ||
         byte(2) == 0xAF) && byte(2) >= 0x80)
        return 3;
    if (c == 0xE2 && byte(1) == 0x81 && byte(2) == 0x9F)
        return 3;
    if (c == 0xE3 && byte(1) == 0x80 && byte(2) == 0x80)
        return 3;
    return 0;
}

}

std::string normalised_words(std::string_view text) {
    std::string out;
    bool pending = false;
    for (std::size_t i = 0; i < text.size();) {
        if (const auto n = space_at(text, i); n > 0) {
            pending = !out.empty();
            i += n;
            continue;
        }
        if (pending)
            out.push_back(' ');
        pending = false;
        out.push_back(text[i++]);
    }
    return out;
}

}

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
#include "compass.platform/text/decoding.hpp"

#include <cstddef>

namespace compass::platform::text {

namespace {

/**
 * @brief The length of the UTF-8 character at @p i that Python's strict
 * decoder accepts, or 0 when the bytes there are not one.
 */
std::size_t character_at(std::string_view text, std::size_t i) noexcept {
    const auto byte = [&](std::size_t k) {
        return static_cast<unsigned char>(text[k]);
    };
    const auto lead = byte(i);
    if (lead < 0x80)
        return 1;
    std::size_t length = 0;
    unsigned char low = 0x80;
    unsigned char high = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        length = 3;
        if (lead == 0xE0)
            low = 0xA0;
        else if (lead == 0xED)
            high = 0x9F;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        length = 4;
        if (lead == 0xF0)
            low = 0x90;
        else if (lead == 0xF4)
            high = 0x8F;
    } else {
        return 0;
    }
    if (i + length > text.size())
        return 0;
    if (byte(i + 1) < low || byte(i + 1) > high)
        return 0;
    for (std::size_t k = 2; k < length; ++k) {
        if ((byte(i + k) & 0xC0) != 0x80)
            return 0;
    }
    return length;
}


}

bool is_utf8(std::string_view text) noexcept {
    for (std::size_t i = 0; i < text.size();) {
        if (static_cast<unsigned char>(text[i]) < 0x80) {
            ++i;
            continue;
        }
        const auto n = character_at(text, i);
        if (n == 0)
            return false;
        i += n;
    }
    return true;
}

namespace {

std::string decode(std::string_view text, std::string_view replacement) {
    if (is_utf8(text))
        return std::string(text);
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        const auto n = character_at(text, i);
        if (n == 0) {
            out += replacement;
            ++i;
        } else {
            out.append(text.substr(i, n));
            i += n;
        }
    }
    return out;
}

}

std::string decode_ignoring(std::string_view text) {
    return decode(text, {});
}

std::string decode_replacing(std::string_view text) {
    return decode(text, "\xEF\xBF\xBD");
}

std::string universal_newlines(std::string_view text) {
    if (text.find('\r') == std::string_view::npos)
        return std::string(text);
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\r') {
            out.push_back(text[i]);
            continue;
        }
        out.push_back('\n');
        if (i + 1 < text.size() && text[i + 1] == '\n')
            ++i;
    }
    return out;
}

}

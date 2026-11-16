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

}

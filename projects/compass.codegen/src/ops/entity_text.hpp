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
#ifndef COMPASS_CODEGEN_OPS_ENTITY_TEXT_HPP
#define COMPASS_CODEGEN_OPS_ENTITY_TEXT_HPP

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Python's str methods the enrichment steps call, for ASCII text.
 */
namespace compass::codegen::ops::text {

[[nodiscard]] inline std::string upper(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

[[nodiscard]] inline std::string lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

/**
 * @brief str.capitalize(): the first character upper case, the rest lower.
 */
[[nodiscard]] inline std::string capitalize(std::string_view s) {
    auto out = lower(s);
    if (!out.empty())
        out[0] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

/**
 * @brief str.title(): upper case after any character that is not a
 * letter, lower case after a letter.
 */
[[nodiscard]] inline std::string title(std::string_view s) {
    std::string out(s);
    bool after_letter = false;
    for (auto& c : out) {
        const auto u = static_cast<unsigned char>(c);
        c = static_cast<char>(after_letter ? std::tolower(u) : std::toupper(u));
        after_letter = std::isalpha(u) != 0;
    }
    return out;
}

/**
 * @brief str.split(separator).
 */
[[nodiscard]] inline std::vector<std::string_view> split(std::string_view s,
    char separator) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    for (auto end = s.find(separator); end != std::string_view::npos;
         end = s.find(separator, start)) {
        out.push_back(s.substr(start, end - start));
        start = end + 1;
    }
    out.push_back(s.substr(start));
    return out;
}

/**
 * @brief re.sub(rf"\b{word}\b", replacement, s): each occurrence of @p word
 * that no word character touches on either side replaced.
 */
[[nodiscard]] inline std::string replace_word(std::string_view s,
    std::string_view word, std::string_view replacement) {
    const auto is_word = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_';
    };
    std::string out;
    std::size_t from = 0;
    for (auto at = s.find(word); at != std::string_view::npos;
         at = s.find(word, at + 1)) {
        const auto end = at + word.size();
        if ((at > 0 && is_word(s[at - 1])) ||
            (end < s.size() && is_word(s[end])) || at < from)
            continue;
        out.append(s.substr(from, at - from));
        out.append(replacement);
        from = end;
    }
    out.append(s.substr(from));
    return out;
}

/**
 * @brief snake_to_pascal: each word of a snake_case name capitalised,
 * joined.
 */
[[nodiscard]] inline std::string snake_to_pascal(std::string_view name) {
    std::string out;
    for (const auto w : split(name, '_'))
        out += capitalize(w);
    return out;
}

/**
 * @brief A snake_case name as the label templates show: its words
 * capitalised, id, iso and fx in capitals, joined by spaces.
 */
[[nodiscard]] inline std::string render_label(std::string_view name) {
    std::string out;
    const auto words = split(name, '_');
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i != 0)
            out += ' ';
        const auto w = lower(words[i]);
        out += w == "id" || w == "iso" || w == "fx" ? upper(w) : capitalize(w);
    }
    return out;
}

}

#endif

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
#include "compass.codec/ops/hand_edits.hpp"

#include <charconv>
#include <unordered_set>
#include "compass.codec/ops/unified_diff.hpp"

namespace compass::codec::ops {

namespace {

std::string_view trimmed(std::string_view line) noexcept {
    constexpr std::string_view space = " \t\r\f\v";
    const auto first = line.find_first_not_of(space);
    if (first == std::string_view::npos)
        return {};
    return line.substr(first, line.find_last_not_of(space) + 1 - first);
}

/**
 * @brief The first line of the old side of the hunk header @p header,
 * "@@ -<line>[,<count>] +... @@".
 */
std::uint32_t old_start(std::string_view header) noexcept {
    std::uint32_t line = 0;
    const auto digits = header.substr(4);
    std::from_chars(digits.data(), digits.data() + digits.size(), line);
    return line;
}

}

std::optional<dropped_block> dropped_hand_lines(std::string_view on_disk,
    std::string_view generated) {
    std::unordered_set<std::string_view> anywhere;
    for (auto rest = generated; !rest.empty();) {
        const auto end = rest.find('\n');
        anywhere.insert(trimmed(rest.substr(0, end)));
        rest = end == std::string_view::npos ? std::string_view{}
                                             : rest.substr(end + 1);
    }

    const auto diff = unified_diff("f", on_disk, generated, 0);
    dropped_block best;
    dropped_block hunk;
    std::int64_t net = 0;
    std::int64_t best_net = 0;
    const auto close = [&] {
        if (net > best_net) {
            best_net = net;
            best = hunk;
        }
    };
    std::uint32_t old_line = 0;
    std::string_view rest = diff;
    for (int header = 0; header < 2 && !rest.empty(); ++header) {
        const auto end = rest.find('\n');
        rest = end == std::string_view::npos ? std::string_view{}
                                             : rest.substr(end + 1);
    }
    while (!rest.empty()) {
        const auto end = rest.find('\n');
        const auto line = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view{}
                                             : rest.substr(end + 1);
        if (line.starts_with("@@ ")) {
            close();
            hunk = {};
            net = 0;
            old_line = old_start(line);
            continue;
        }
        if (line.empty())
            continue;
        const auto text = trimmed(line.substr(1));
        if (line[0] == '+') {
            net -= text.empty() ? 0 : 1;
        } else if (line[0] == '-') {
            const auto at = old_line++;
            if (text.empty() || anywhere.contains(text))
                continue;
            ++net;
            if (hunk.lines++ == 0) {
                hunk.line = at;
                hunk.first = std::string(text);
            }
        }
    }
    close();
    if (best_net < static_cast<std::int64_t>(dropped_block_minimum))
        return std::nullopt;
    best.lines = static_cast<std::uint32_t>(best_net);
    return best;
}

}

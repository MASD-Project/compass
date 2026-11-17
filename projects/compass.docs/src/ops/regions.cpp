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
#include "compass.docs/ops/regions.hpp"

#include "compass.platform/text/regex.hpp"

namespace compass::docs::ops {

std::optional<std::string> replace_region(std::string_view text,
    std::string_view begin, std::string_view end, std::string_view block,
    replace which) {
    namespace pt = compass::platform::text;
    const pt::regex region(pt::escape(begin) + ".*?" + pt::escape(end),
        {.dotall = true});
    const auto m = region.search(text);
    if (!m)
        return std::nullopt;
    if (which == replace::every)
        return region.substitute(text, block);
    return std::string(text.substr(0, m->start())) + std::string(block) +
        std::string(text.substr(m->end()));
}

std::string join_lines(const std::vector<std::string>& lines) {
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0)
            out.push_back('\n');
        out += lines[i];
    }
    return out;
}

}

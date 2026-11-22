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
#include "compass.docs/ops/skills.hpp"

#include <algorithm>

namespace compass::docs::ops {

std::vector<std::string> skill_names(
    const compass::codec::types::corpus_view& v) {
    return skill_names(v, skills_dir);
}

std::vector<std::string> skill_names(
    const compass::codec::types::corpus_view& v, std::string_view directory) {
    constexpr std::string_view file = "/SKILL.org";
    std::vector<std::string> out;
    for (const auto path : v.list_files(directory, file)) {
        const auto name = path.substr(directory.size(),
            path.size() - directory.size() - file.size());
        if (name.find('/') == std::string_view::npos)
            out.emplace_back(name);
    }
    std::ranges::sort(out);
    return out;
}

}

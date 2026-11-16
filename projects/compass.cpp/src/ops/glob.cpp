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
#include "compass.cpp/ops/glob.hpp"

#include <algorithm>
#include "compass.cpp/ops/paths.hpp"

namespace compass::cpp::ops {

std::vector<std::string_view> glob(
    const compass::codec::types::corpus_view& v, std::string_view pattern) {
    const auto wild = pattern.find_first_of("*?");
    const auto slash = pattern.rfind('/', wild);
    const auto prefix = slash == std::string_view::npos
        ? std::string_view{} : pattern.substr(0, slash + 1);
    const auto last = pattern.find_last_of("*?/");
    const auto suffix = last == std::string_view::npos
        ? pattern : pattern.substr(last + 1);
    auto out = v.list_files(prefix, suffix);
    std::erase_if(out, [&](auto p) { return !glob_matches(pattern, p); });
    std::ranges::sort(out, component_less);
    return out;
}

}

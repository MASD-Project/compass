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
#include "compass.cpp/ops/paths.hpp"

#include <algorithm>

namespace compass::cpp::ops {

namespace {

std::string_view name_of(std::string_view path) noexcept {
    const auto slash = path.rfind('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

}

std::string_view suffix_of(std::string_view path) noexcept {
    const auto name = name_of(path);
    const auto rest = name.substr(std::min(name.find_first_not_of('.'),
        name.size()));
    const auto dot = rest.rfind('.');
    return dot == std::string_view::npos ? std::string_view{}
                                         : rest.substr(dot);
}

std::string_view stem_of(std::string_view path) noexcept {
    const auto name = name_of(path);
    return name.substr(0, name.size() - suffix_of(path).size());
}

std::string_view parent_of(std::string_view path) noexcept {
    const auto slash = path.rfind('/');
    return slash == std::string_view::npos ? std::string_view{}
                                           : path.substr(0, slash);
}

std::vector<std::string_view> components_of(std::string_view path) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (start <= path.size()) {
        const auto slash = path.find('/', start);
        const auto end = slash == std::string_view::npos ? path.size()
                                                         : slash;
        out.push_back(path.substr(start, end - start));
        if (slash == std::string_view::npos)
            break;
        start = slash + 1;
    }
    return out;
}

bool component_less(std::string_view a, std::string_view b) noexcept {
    for (;;) {
        const auto sa = a.find('/');
        const auto sb = b.find('/');
        const auto ca = a.substr(0, sa);
        const auto cb = b.substr(0, sb);
        if (ca != cb)
            return ca < cb;
        if (sa == std::string_view::npos || sb == std::string_view::npos)
            return sa == std::string_view::npos &&
                sb != std::string_view::npos;
        a.remove_prefix(sa + 1);
        b.remove_prefix(sb + 1);
    }
}

bool file_set::is_file(std::string_view path) const noexcept {
    return std::ranges::binary_search(paths_, path);
}

bool file_set::is_dir(std::string_view path) const {
    if (path.empty())
        return true;
    std::string prefix(path);
    prefix.push_back('/');
    const auto it = std::ranges::lower_bound(paths_,
        std::string_view(prefix));
    return it != paths_.end() && it->starts_with(prefix);
}

std::optional<std::string> file_set::find(std::string_view dir,
    std::string_view name) const {
    if (name.starts_with('/'))
        return std::nullopt;
    std::string path(dir);
    for (const auto part : components_of(name)) {
        if (part.empty() || part == ".")
            continue;
        if (part == "..") {
            if (path.empty() || !is_dir(path))
                return std::nullopt;
            const auto slash = path.rfind('/');
            path.erase(slash == std::string::npos ? 0 : slash);
            continue;
        }
        if (!path.empty())
            path.push_back('/');
        path += part;
    }
    if (!is_file(path))
        return std::nullopt;
    return path;
}

}

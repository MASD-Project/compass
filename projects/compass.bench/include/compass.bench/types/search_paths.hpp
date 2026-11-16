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
#ifndef COMPASS_BENCH_TYPES_SEARCH_PATHS_HPP
#define COMPASS_BENCH_TYPES_SEARCH_PATHS_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace compass::bench::types {

/**
 * @brief An include directory.
 *
 * An internal directory lies in the checkout and its path is relative to the
 * checkout root, with no trailing slash; the root itself is the empty
 * string. An external directory keeps its absolute path.
 */
struct search_directory {
    std::string path;
    bool internal = false;
};

/**
 * @brief The ordered directories one file's includes are searched in.
 *
 * directories[0, first_angle) are the -iquote directories, searched only by
 * quoted includes. directories[first_angle, end) are the -I, then -isystem,
 * then implicit system directories, searched by both kinds.
 */
struct search_context {
    std::vector<std::uint32_t> directories;
    std::size_t first_angle = 0;
};

/**
 * @brief Search directories, contexts, and the context of every file.
 *
 * file_contexts is indexed by file id and holds an index into contexts.
 */
struct search_paths {
    std::vector<search_directory> directories;
    std::vector<search_context> contexts;
    std::vector<std::uint32_t> file_contexts;
};

}

#endif

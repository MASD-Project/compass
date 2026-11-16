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
#ifndef COMPASS_BENCH_OPS_SEARCH_PATHS_HPP
#define COMPASS_BENCH_OPS_SEARCH_PATHS_HPP

#include <filesystem>
#include <span>
#include <string>
#include "compass.bench/types/compile_command.hpp"
#include "compass.bench/types/file_table.hpp"
#include "compass.bench/types/search_paths.hpp"

namespace compass::bench::ops {

/**
 * @brief Derives the search context of every file in @p table.
 *
 * Each command contributes its -iquote, -I and -isystem directories, joined
 * or separate, made absolute against its directory and lexically normalised.
 * A directory under the command's build directory stays absolute and is
 * external: the build tree is not part of the checkout. Otherwise a
 * directory under @p source_root is rebased onto the checkout and is
 * internal, and any other directory stays absolute and is external.
 * @p implicit_system directories follow every command's -isystem ones.
 *
 * Each command is anchored at the checkout directory it compiles: the
 * directory of its file, or, for a file generated in the build tree such as
 * a CMake unity source, the build-tree directory above "CMakeFiles". A file
 * takes the union of the commands anchored at or below the nearest ancestor
 * of its directory that has any, so a source file gets its own target's
 * directories and a header gets those of every target in its component.
 */
[[nodiscard]] types::search_paths build_search_paths(
    std::span<const types::compile_command> commands,
    const std::filesystem::path& source_root,
    std::span<const std::string> implicit_system,
    const types::file_table& table);

}

#endif

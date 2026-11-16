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
#ifndef COMPASS_BENCH_OPS_LISTING_HPP
#define COMPASS_BENCH_OPS_LISTING_HPP

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.bench/types/file_table.hpp"

namespace compass::bench::ops {

/**
 * @brief Runs one `git -C <checkout> ls-files -z` and returns its raw output.
 *
 * Throws std::system_error if git cannot be spawned and std::runtime_error if
 * it exits with a non-zero status.
 */
[[nodiscard]] std::string git_ls_files(const std::filesystem::path& checkout);

/**
 * @brief The IDs `git hash-object --no-filters --stdin-paths` prints for
 * @p paths, relative to @p checkout, in order: the IDs of the bytes on
 * disk, with no conversion. A path must not hold a newline nor start with
 * a double quote.
 *
 * git reads the paths from a temporary file and writes nothing to the
 * repository. Throws as git_ls_files does, and std::runtime_error if git
 * prints a different number of IDs.
 */
[[nodiscard]] std::vector<std::string> git_hash_objects(
    const std::filesystem::path& checkout,
    std::span<const std::string_view> paths);

/**
 * @brief Splits NUL-terminated git output into paths that view into @p raw.
 */
[[nodiscard]] std::vector<std::string_view> parse_listing(std::string_view raw);

/**
 * @brief The extension of the last path segment, including the dot.
 *
 * Empty when the name has no dot or only a leading one (".gitignore").
 */
[[nodiscard]] std::string_view extension_of(std::string_view path);

/**
 * @brief Appends a file, interning its extension.
 */
void add_file(types::file_table& table, std::string_view path,
    std::uint64_t size);

/**
 * @brief Builds the table for the listed paths under @p checkout.
 *
 * Entries that exist but are not regular files (submodules, symlinks) are
 * skipped. Entries that cannot be stat'ed are kept with size zero so the
 * read stage reports them as errors rather than hiding them.
 */
[[nodiscard]] types::file_table build_file_table(
    const std::filesystem::path& checkout,
    std::span<const std::string_view> entries);

}

#endif

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
#ifndef COMPASS_CODEGEN_OPS_LIBRARY_HPP
#define COMPASS_CODEGEN_OPS_LIBRARY_HPP

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codegen/ops/generate.hpp"

namespace compass::codegen::ops {

/**
 * @brief The name a library option gives the MASD library that ships with
 * compass.
 */
inline constexpr std::string_view compass_library = "@compass";

/**
 * @brief The directory of the MASD library that ships with the compass at
 * @p executable: share/compass/masd beside the directory that holds it,
 * as the package, a build tree and install_compass.sh lay it out.
 */
[[nodiscard]] std::filesystem::path compass_library_directory(
    const std::filesystem::path& executable);

/**
 * @brief The directories of the comma-separated library option @p option,
 * in order: @c @compass as compass_library_directory of @p executable, an
 * absolute path as it is, both outside the checkout, and any other path
 * relative to the checkout. A directory is in generic form, with no
 * trailing slash.
 * @throws std::runtime_error when the option or one of its entries is
 * empty, or when it names @c @compass and @p executable is unknown.
 */
[[nodiscard]] std::vector<std::string> library_directories(
    std::string_view option, const std::filesystem::path& executable);

/**
 * @brief Whether the library directory @p directory is outside the
 * checkout, and is read from disk.
 */
[[nodiscard]] bool outside_the_checkout(std::string_view directory);

/**
 * @brief @p checkout, which reads the checkout, extended to read the
 * files of an absolute path from disk, as the libraries outside the
 * checkout are. Listing such a path lists the files below the directory
 * its prefix names.
 */
[[nodiscard]] checkout_reader with_files_outside(checkout_reader checkout);

/**
 * @brief The path and content ID of every file of the libraries among
 * @p directories that are outside the checkout, in path order, which key
 * the codec's configuration since a run cannot record them as inputs.
 * A directory that cannot be read gives nothing.
 */
[[nodiscard]] std::string outside_files_digest(
    const std::vector<std::string>& directories);

}

#endif

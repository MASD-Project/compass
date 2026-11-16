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
#ifndef COMPASS_CLI_CONFIG_REPOSITORY_FILE_HPP
#define COMPASS_CLI_CONFIG_REPOSITORY_FILE_HPP

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include "compass.cli/types/configuration.hpp"

namespace compass::cli::config {

/**
 * @brief The name of the configuration file at the root of a checkout.
 * It configures the run and is not part of the corpus.
 */
inline constexpr std::string_view repository_file_name = ".compass.org";

/**
 * @brief The layer the org document @p text sets, or the first error in
 * it as "<label>:<line>: <what>; fix: <how>".
 *
 * #+output: sets the output root and #+meta: the contract directory. The
 * first table under the level-1 heading "* Codecs" has one codec per row:
 * name, enabled as yes or no, and options as space-separated key=value
 * pairs. A first row whose first cell is "name" is a header; separator
 * lines are skipped. A row with other than three cells, an enabled value
 * other than yes or no, a pair with no key or no "=", the key enabled
 * among the options, a codec named twice and a document with no such
 * table are errors. Codec and option names are not checked here.
 */
[[nodiscard]] std::expected<types::layer, std::string> parse_repository_file(
    std::string_view text, std::string_view label);

/**
 * @brief The layer the configuration file of @p checkout sets: an empty
 * layer when the file does not exist, else as parse_repository_file
 * reads it.
 */
[[nodiscard]] std::expected<types::layer, std::string> read_repository_file(
    const std::filesystem::path& checkout);

}

#endif

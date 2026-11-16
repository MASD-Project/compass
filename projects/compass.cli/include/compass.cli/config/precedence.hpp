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
#ifndef COMPASS_CLI_CONFIG_PRECEDENCE_HPP
#define COMPASS_CLI_CONFIG_PRECEDENCE_HPP

#include <filesystem>
#include <span>
#include <string_view>
#include "compass.cli/types/configuration.hpp"
#include "compass.codec/types/configuration.hpp"
#include "compass.codec/types/descriptor.hpp"

namespace compass::cli::config {

/**
 * @brief The paths a run uses when no layer sets them, relative to the
 * checkout.
 */
inline constexpr std::string_view default_meta = "doc/meta";
inline constexpr std::string_view default_output = "build/output/compass";

/**
 * @brief @p p under @p base, normalised, with no trailing separator; @p p
 * itself when it is absolute.
 */
[[nodiscard]] std::filesystem::path absolute_under(
    const std::filesystem::path& base, const std::filesystem::path& p);

/**
 * @brief The configuration of a run: the defaults of each codec in
 * @p catalogue and of compass, then @p from_file, then @p command_line,
 * the later value winning key by key.
 *
 * The paths come out absolute under @p checkout. @p file is the
 * configuration file the file origins point into, named relative to the
 * checkout in each problem. A setting for a codec the catalogue lacks,
 * for a key the codec does not declare, or an enabled value other than
 * true or false is a problem, and leaves the configuration as it would be
 * without it.
 */
[[nodiscard]] types::configuration resolve(
    std::span<const codec::types::descriptor> catalogue,
    const std::filesystem::path& checkout, const std::filesystem::path& file,
    const types::layer& from_file, const types::layer& command_line);

/**
 * @brief Every setting of @p c, enabled and each option of each codec,
 * as the codec registry takes them.
 */
[[nodiscard]] codec::types::configuration settings_of(
    const types::configuration& c);

}

#endif

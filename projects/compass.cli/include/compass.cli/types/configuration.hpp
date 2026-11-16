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
#ifndef COMPASS_CLI_TYPES_CONFIGURATION_HPP
#define COMPASS_CLI_TYPES_CONFIGURATION_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace compass::cli::types {

/**
 * @brief Where a value came from: the codec or compass default, the
 * repository's configuration file, or the command line.
 */
enum class origin_kind : std::uint8_t { defaults, file, command_line };

/**
 * @brief Where a value came from; line is the 1-based line of the
 * configuration file, and 0 for any other origin.
 */
struct origin {
    origin_kind kind = origin_kind::defaults;
    std::uint32_t line = 0;
};

/**
 * @brief One codec setting as a layer wrote it. The key enabled holds
 * true or false; any other key names an option of the codec.
 */
struct setting {
    std::string codec;
    std::string key;
    std::string value;
    origin from;
};

/**
 * @brief A path and where it came from.
 */
struct sourced_path {
    std::filesystem::path value;
    origin from;
};

/**
 * @brief What one source of configuration sets, as written: the output
 * root and the contract directory, relative to the checkout unless
 * absolute, and codec settings in the order they appear.
 */
struct layer {
    std::optional<sourced_path> output;
    std::optional<sourced_path> meta;
    std::vector<setting> settings;
};

/**
 * @brief A value and where it came from.
 */
struct sourced_value {
    std::string value;
    origin from;
};

/**
 * @brief The effective configuration of one codec: enabled as true or
 * false, and one value per option the codec declares, in its order.
 */
struct codec_configuration {
    std::string name;
    sourced_value enabled;
    std::vector<std::string> keys;
    std::vector<sourced_value> options;
};

/**
 * @brief The configuration a run uses once every layer is applied.
 *
 * file is the configuration file the file origins point into. output and
 * meta are absolute. codecs has one row per codec of the catalogue, in
 * catalogue order. problems names each setting that matches no codec or
 * no option; any problem fails the command.
 */
struct configuration {
    std::filesystem::path file;
    sourced_path output;
    sourced_path meta;
    std::vector<codec_configuration> codecs;
    std::vector<std::string> problems;
};

}

#endif

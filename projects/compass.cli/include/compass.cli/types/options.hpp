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
#ifndef COMPASS_CLI_TYPES_OPTIONS_HPP
#define COMPASS_CLI_TYPES_OPTIONS_HPP

#include <cstdint>
#include <filesystem>
#include "compass.codec/types/configuration.hpp"
#include "compass.store/types/collection.hpp"

namespace compass::cli::types {

/**
 * @brief The commands of the compass executable.
 */
enum class command : std::uint8_t { generate, diff, check, gc };

/**
 * @brief What one command runs over and how.
 *
 * checkout, meta and output_root are absolute. settings holds every
 * --set, in order, and collection the options of gc, which no other
 * command reads.
 */
struct options {
    command which = command::generate;
    std::filesystem::path checkout;
    std::filesystem::path meta;
    std::filesystem::path output_root;
    unsigned threads = 1;
    codec::types::configuration settings;
    store::types::collection_options collection;
};

}

#endif

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
#include <optional>
#include <string>
#include "compass.cli/types/configuration.hpp"
#include "compass.codec/types/configuration.hpp"
#include "compass.store/types/collection.hpp"
#include "compass.store/types/git_object.hpp"

namespace compass::cli::types {

/**
 * @brief The commands of the compass executable.
 */
enum class command : std::uint8_t { generate, diff, check, gc, config };

/**
 * @brief What the command line asked for, before the configuration file
 * is read.
 *
 * checkout is absolute. command_line holds --out, --meta and every
 * --set, as written, and collection the options of gc. timings asks for
 * the time of each step and what each codec ran or reused. revision is
 * the --commit of check as written, or empty. ratchet, when set, is the
 * base of check --ratchet as written, empty for the default.
 */
struct invocation {
    command which = command::generate;
    std::filesystem::path checkout;
    std::string revision;
    std::optional<std::string> ratchet;
    unsigned threads = 1;
    bool timings = false;
    layer command_line;
    store::types::collection_options collection;
};

/**
 * @brief What one command runs over and how, once every layer of
 * configuration is applied.
 *
 * checkout, meta and output_root are absolute. settings holds the value
 * of every codec setting, enabled and options, and collection the
 * options of gc, which no other command reads. commit, when set, is the
 * commit check reads from git's object database instead of the files of
 * the checkout; meta and output_root then name directories of that
 * commit, and the checkout only locates the repository, its store, and
 * the programs and style files of formatters.
 */
struct options {
    command which = command::generate;
    std::filesystem::path checkout;
    std::optional<store::types::object_id> commit;
    std::filesystem::path meta;
    std::filesystem::path output_root;
    unsigned threads = 1;
    codec::types::configuration settings;
    store::types::collection_options collection;
};

}

#endif

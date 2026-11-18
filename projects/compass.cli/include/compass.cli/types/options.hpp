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

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "compass.cli/config/check_documents.hpp"
#include "compass.cli/types/configuration.hpp"
#include "compass.codec/types/configuration.hpp"
#include "compass.codec/types/exception.hpp"
#include "compass.store/types/collection.hpp"
#include "compass.store/types/git_object.hpp"

namespace compass::cli::types {

/**
 * @brief The commands of the compass executable.
 */
enum class command : std::uint8_t { generate, diff, check, gc, config, hook };

/**
 * @brief What the command line asked for, before the configuration file
 * is read.
 *
 * checkout is absolute. command_line holds --out, --meta and every
 * --set, as written, and collection the options of gc. timings asks for
 * the time of each step and what each codec ran or reused. revision is
 * the --commit of check as written, or empty. ratchet, when set, is the
 * base of check --ratchet as written, empty for the default. today is
 * the --today of check, when given. all_findings is the --all-findings of
 * hook. only holds each --codec of check: when it is not empty, the
 * codecs and checks it names run and no other does.
 */
struct invocation {
    command which = command::generate;
    std::filesystem::path checkout;
    std::string revision;
    std::optional<std::string> ratchet;
    std::optional<std::chrono::year_month_day> today;
    unsigned threads = 1;
    bool timings = false;
    bool all_findings = false;
    std::vector<std::string> only;
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
 * the programs and style files of formatters. today is the date check
 * judges the expiry of exceptions against, for a commit as for the
 * checkout. exceptions are those the exceptions table of the
 * configuration file in effect states, applied with the exception
 * comments of the files. all_findings makes hook report every finding on
 * the edited file, not only those the last check did not record.
 * checks are the checks the project's check documents define, at the
 * commit, or in the checkout when there is no commit or it has none, so
 * today's checks can judge an old commit; their exceptions are among
 * exceptions.
 */
struct options {
    command which = command::generate;
    std::filesystem::path checkout;
    std::optional<store::types::object_id> commit;
    std::filesystem::path meta;
    std::filesystem::path output_root;
    unsigned threads = 1;
    std::chrono::year_month_day today;
    codec::types::configuration settings;
    codec::types::exception_set exceptions;
    std::shared_ptr<const config::check_set> checks;
    store::types::collection_options collection;
    bool all_findings = false;
};

}

#endif

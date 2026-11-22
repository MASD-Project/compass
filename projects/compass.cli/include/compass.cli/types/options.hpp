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
#include <utility>
#include <vector>
#include "compass.cli/config/check_documents.hpp"
#include "compass.cli/types/configuration.hpp"
#include "compass.cli/types/results.hpp"
#include "compass.codec/types/configuration.hpp"
#include "compass.codec/types/exception.hpp"
#include "compass.store/types/collection.hpp"
#include "compass.store/types/git_object.hpp"

namespace compass::cli::types {

/**
 * @brief The commands of the compass executable.
 */
enum class command : std::uint8_t {
    generate, diff, check, gc, config, hook, env, blame, sync, db
};

/**
 * @brief What compass env does: write the environment file, print the
 * format's version, record a new version, or say whether the file is
 * current.
 */
enum class env_action : std::uint8_t { generate, version, version_new, status };

/**
 * @brief The arguments of compass env.
 *
 * inputs holds each --input and variables each --var, as name and value;
 * file is --file, the environment file in place of the format's own;
 * description, adds, removes and format_only are those of version new.
 */
struct env_request {
    env_action action = env_action::status;
    std::vector<std::pair<std::string, std::string>> inputs;
    std::vector<std::pair<std::string, std::string>> variables;
    std::string file;
    std::string description;
    std::vector<std::string> adds;
    std::vector<std::string> removes;
    bool format_only = false;
};

/**
 * @brief What compass db does: create or migrate the schema, say where it
 * stands, install the git hooks that sync, or drop environments and
 * collect.
 */
enum class db_action : std::uint8_t {
    init, migrate, status, hooks_install, prune
};

/**
 * @brief The arguments of compass db: the action, the --database
 * connection string init records, and the --text-search configuration it
 * creates the database with, empty for the default.
 */
struct db_request {
    db_action action = db_action::status;
    std::string database;
    std::string text_search;
};

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
 * codecs and checks it names run and no other does. blame is the
 * --blame or --no-blame of check, bisect_from the --bisect-from of check
 * and blame, and target the finding blame traces, as written. scope holds
 * each --scope of generate and diff as written, skip_stale is their
 * --skip-stale and list_files their --files. env holds the arguments of
 * compass env. environment is the --environment of sync and db prune,
 * and db the arguments of compass db.
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
    blame_mode blame = blame_mode::cheap;
    std::string bisect_from;
    std::string target;
    std::vector<std::string> scope;
    bool skip_stale = false;
    bool list_files = false;
    layer command_line;
    store::types::collection_options collection;
    env_request env;
    std::string environment;
    db_request db;
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
 * exceptions. blame says which blame methods check runs on the findings
 * already on main, and bisect_from is the commit bisect starts from,
 * empty for the oldest of the last merges it walks. scope narrows generate
 * and diff to the outputs of the units that read one of its paths, each
 * relative to the checkout, and to what those outputs feed, pass by pass;
 * empty is every output. skip_stale makes them leave alone the files that
 * were already stale at HEAD.
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
    blame_mode blame = blame_mode::cheap;
    std::string bisect_from;
    std::vector<std::string> scope;
    bool skip_stale = false;
};

}

#endif

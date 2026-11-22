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
#include "compass.cli/config/arguments.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include "compass.cli/config/precedence.hpp"
#include "compass.codec/ops/exceptions.hpp"

namespace compass::cli::config {

namespace {

constexpr std::string_view help =
    "usage: compass [-C <dir>] <command> [<options>]\n"
    "\n"
    "commands:\n"
    "  generate, gen   write the codec outputs that changed and print a "
    "summary\n"
    "  diff            print what generate would change as unified diffs; "
    "write nothing\n"
    "  check           print the diagnostics of every codec and each file "
    "generate would change\n"
    "  gc              remove old unreferenced objects from the store\n"
    "  config          print each codec setting and path, and where it came "
    "from\n"
    "  hook            check the file a Claude Code PostToolUse payload on "
    "standard input names\n"
    "  blame <path>:<line>:<codec>/<rule>\n"
    "                  trace one finding to the commit, pull request and "
    "environment that likely caused it\n"
    "  env generate    write the environment file the check document of "
    "env_format describes\n"
    "  env version     print the environment file format's version\n"
    "  env version new <description>\n"
    "                  record a new version of the format\n"
    "  env status      say whether the environment file is current; exit 1 "
    "when it is not\n"
    "  sync            load this worktree's documents into compass's "
    "database\n"
    "  db init         create compass's database schema and record the "
    "connection\n"
    "  db migrate      bring the database's schema up to this compass's\n"
    "  db status       print the schema version, environments and sizes; "
    "exit 1 when the schema is not current\n"
    "  db hooks install\n"
    "                  install git hooks that sync after a checkout, merge "
    "or rebase\n"
    "  db prune        drop the environments whose worktree is gone, then "
    "collect\n"
    "\n"
    "options:\n"
    "  -C <dir>                    the checkout (default: the current "
    "directory)\n"
    "  --meta <dir>                the contract pages, under the checkout "
    "(default: #+meta: of .compass.org, else doc/meta)\n"
    "  --out <dir>                 the output root, under the checkout "
    "(default: #+output: of .compass.org, else build/output/compass)\n"
    "  -j <n>                      threads (default: one per core)\n"
    "  --set <codec>.<key>=<value> set a codec option over .compass.org; "
    "repeatable\n"
    "  --timings                   generate, diff, check, hook: print each step's "
    "time and what each codec ran or reused\n"
    "  --scope <path>              generate, diff: only the outputs of the "
    "units that read this file, a template, model or any input, and what "
    "they feed; repeatable\n"
    "  --skip-stale                generate, diff: leave alone the files "
    "already stale at HEAD, which no change since HEAD caused\n"
    "  --files                     generate, diff: list every changed file, "
    "not counts by what produced them, when many change\n"
    "  --codec <name>              check: run only this codec or check, "
    "whatever .compass.org enables; repeatable\n"
    "  --commit <rev>              check: check this commit, read from git's "
    "objects, not the files on disk\n"
    "                              sync: sync this commit, as a release "
    "environment\n"
    "  --environment <name>        sync: the environment to sync (default: "
    "#+environment: of the local settings, else the worktree's directory)\n"
    "                              db prune: drop this environment\n"
    "  --database <conninfo>       db init: the libpq connection string to "
    "record in the repository's local settings\n"
    "  --text-search <config>      db init: the text search configuration "
    "(default: english)\n"
    "  --ratchet [<base>]          check: fail only on diagnostics the merge "
    "base of HEAD and <base> lacks (default: the upstream, else "
    "origin/main)\n"
    "  --blame[=bisect]            check: blame the findings already on "
    "main by line, added and drift (default), or also bisect main's merges\n"    "                              diff --blame: name the commits that made "
    "each stale file, when at most twenty are\n"
    "  --no-blame                  check: blame nothing\n"
    "  --bisect-from <rev>         check, blame: the commit bisect starts "
    "from (default: the oldest of the last 128 merges)\n"
    "  --today <YYYY-MM-DD>        check, hook, blame: judge the expiry of exceptions "
    "as of this date (default: today in UTC)\n"
    "  --all-findings              hook: report every finding on the edited "
    "file, not only those the last check did not record\n"
    "  --dry-run                   gc: remove nothing, report what would go\n"
    "  --cap <bytes>               gc: the size the store comes down to "
    "(default: 0)\n"
    "  --grace <seconds>           gc: never remove a file younger than this "
    "(default: 3600)\n"
    "  --input <name>=<value>      env generate: give an input the format "
    "declares; repeatable\n"
    "  --var <NAME>=<value>        env generate: set a variable outright; "
    "repeatable\n"
    "  --file <path>               env generate, status: the environment "
    "file, under the checkout (default: the format's file setting)\n"
    "  --adds <A,B>                env version new: the variables the "
    "version adds; repeatable\n"
    "  --removes <C>               env version new: the variables the "
    "version removes; repeatable\n"
    "  --format-only               env version new: the version changes "
    "only values, layout or the services the registry expands\n"
    "  -h, --help                  print this help\n"
    "  --version                   print the version\n"
    "\n"
    "exit status: 0 when nothing would change and no diagnostic fires, 1 "
    "when\n"
    "diff finds a change or check a diagnostic, 2 on failure or bad usage.\n";

std::optional<types::command> command_named(std::string_view name) {
    if (name == "generate" || name == "gen")
        return types::command::generate;
    if (name == "diff")
        return types::command::diff;
    if (name == "check")
        return types::command::check;
    if (name == "gc")
        return types::command::gc;
    if (name == "config")
        return types::command::config;
    if (name == "hook")
        return types::command::hook;
    if (name == "blame")
        return types::command::blame;
    if (name == "env")
        return types::command::env;
    if (name == "sync")
        return types::command::sync;
    if (name == "db")
        return types::command::db;
    return std::nullopt;
}

template <typename Number>
std::optional<Number> parse_number(std::string_view s) {
    Number v = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || end != s.data() + s.size())
        return std::nullopt;
    return v;
}

/**
 * @brief Adds the setting written as <codec>.<key>=<value>; false when
 * @p s is not of that form.
 */
bool add_setting(std::string_view s, std::vector<types::setting>& settings) {
    const auto equals = s.find('=');
    const auto dot = s.find('.');
    if (equals == std::string_view::npos || dot == 0 || dot + 1 >= equals)
        return false;
    settings.push_back({.codec = std::string(s.substr(0, dot)),
        .key = std::string(s.substr(dot + 1, equals - dot - 1)),
        .value = std::string(s.substr(equals + 1)),
        .from = {types::origin_kind::command_line}});
    return true;
}

bool takes_value(std::string_view flag) {
    return flag == "-C" || flag == "--meta" || flag == "--out" ||
        flag == "-j" || flag == "--set" || flag == "--cap" ||
        flag == "--grace" || flag == "--commit" || flag == "--today" ||
        flag == "--codec" || flag == "--bisect-from" || flag == "--input" ||
        flag == "--var" || flag == "--file" || flag == "--adds" ||
        flag == "--removes" || flag == "--scope" ||
        flag == "--environment" || flag == "--database" ||
        flag == "--text-search";
}

/**
 * @brief Adds @p s, NAME=value, to @p to; false when it has no '=' or no
 * name.
 */
bool add_pair(std::string_view s,
    std::vector<std::pair<std::string, std::string>>& to) {
    const auto equals = s.find('=');
    if (equals == std::string_view::npos || equals == 0)
        return false;
    to.emplace_back(std::string(s.substr(0, equals)),
        std::string(s.substr(equals + 1)));
    return true;
}

/**
 * @brief Appends the names of @p s, split at commas, to @p to; false when
 * it names none.
 */
bool add_names(std::string_view s, std::vector<std::string>& to) {
    const auto before = to.size();
    std::size_t at = 0;
    while (at <= s.size()) {
        const auto comma = std::min(s.find(',', at), s.size());
        const auto name = s.substr(at, comma - at);
        if (!name.empty())
            to.emplace_back(name);
        at = comma + 1;
    }
    return to.size() > before;
}

/**
 * @brief Takes @p word, a word after env, as the next part of @p r: the
 * action, then new after version, then version new's description.
 */
std::optional<usage_error> env_word(std::string_view word,
    bool& has_action, types::env_request& r) {
    using types::env_action;
    if (!has_action) {
        has_action = true;
        if (word == "generate")
            r.action = env_action::generate;
        else if (word == "version")
            r.action = env_action::version;
        else if (word == "status")
            r.action = env_action::status;
        else
            return usage_error{"unknown env action: " + std::string(word)};
        return std::nullopt;
    }
    if (r.action == env_action::version && word == "new") {
        r.action = env_action::version_new;
        return std::nullopt;
    }
    if (r.action == env_action::version_new && r.description.empty()) {
        r.description = word;
        return std::nullopt;
    }
    return usage_error{"unexpected argument: " + std::string(word)};
}

/**
 * @brief Takes @p word, a word after db, as the next part of @p r: the
 * action, then install after hooks.
 */
std::optional<usage_error> db_word(std::string_view word, bool& has_action,
    bool& has_install, types::db_request& r) {
    using types::db_action;
    if (!has_action) {
        has_action = true;
        if (word == "init")
            r.action = db_action::init;
        else if (word == "migrate")
            r.action = db_action::migrate;
        else if (word == "status")
            r.action = db_action::status;
        else if (word == "hooks")
            r.action = db_action::hooks_install;
        else if (word == "prune")
            r.action = db_action::prune;
        else
            return usage_error{"unknown db action: " + std::string(word)};
        return std::nullopt;
    }
    if (r.action == db_action::hooks_install && !has_install &&
        word == "install") {
        has_install = true;
        return std::nullopt;
    }
    return usage_error{"unexpected argument: " + std::string(word)};
}

/**
 * @brief Why the sync and db arguments of @p o do not fit together, if
 * they do not.
 */
std::optional<usage_error> db_usage(types::command which, bool has_action,
    bool has_install, const types::invocation& o) {
    using types::db_action;
    const bool is_db = which == types::command::db;
    if (is_db && !has_action)
        return usage_error{"db needs an action: init, migrate, status, hooks "
            "install or prune"};
    if (is_db && o.db.action == db_action::hooks_install && !has_install)
        return usage_error{"db hooks needs install"};
    const bool init = is_db && o.db.action == db_action::init;
    if ((!o.db.database.empty() || !o.db.text_search.empty()) && !init)
        return usage_error{"--database and --text-search apply to db init "
            "only"};
    const bool prune = is_db && o.db.action == db_action::prune;
    if (!o.environment.empty() && which != types::command::sync && !prune)
        return usage_error{"--environment applies to sync and db prune only"};
    if (which == types::command::sync && !o.revision.empty() &&
        o.environment.empty())
        return usage_error{"sync --commit needs --environment, the name of "
            "the release"};
    return std::nullopt;
}

/**
 * @brief Why the env arguments of @p o do not fit together, if they do
 * not: env needs an action, version new a description, and each env
 * option its action.
 */
std::optional<usage_error> env_usage(types::command which,
    bool has_action, std::string_view flag, const types::invocation& o) {
    using types::env_action;
    if (which != types::command::env) {
        if (!flag.empty())
            return usage_error{std::string(flag) + " applies to env only"};
        return std::nullopt;
    }
    const auto& r = o.env;
    if (!has_action)
        return usage_error{"env needs an action: generate, version or "
            "status"};
    if (r.action == env_action::version_new && r.description.empty())
        return usage_error{"env version new needs a description"};
    if ((!r.inputs.empty() || !r.variables.empty()) &&
        r.action != env_action::generate)
        return usage_error{"--input and --var apply to env generate only"};
    if (!r.file.empty() && r.action != env_action::generate &&
        r.action != env_action::status)
        return usage_error{"--file applies to env generate and env status "
            "only"};
    if ((!r.adds.empty() || !r.removes.empty() || r.format_only) &&
        r.action != env_action::version_new)
        return usage_error{"--adds, --removes and --format-only apply to env "
            "version new only"};
    return std::nullopt;
}

}

std::string_view help_text() noexcept { return help; }

parsed_arguments parse_arguments(std::span<const std::string_view> args,
    const std::filesystem::path& working_directory,
    unsigned default_threads) {
    if (std::ranges::contains(args, "--help") ||
        std::ranges::contains(args, "-h"))
        return show_help{};
    if (std::ranges::contains(args, "--version"))
        return show_version{};

    types::invocation o;
    o.threads = std::max(1u, default_threads);
    std::optional<types::command> which;
    std::filesystem::path checkout;
    const types::origin command_line{types::origin_kind::command_line};
    std::string_view gc_flag;
    std::string_view blame_flag;
    std::string_view env_flag;
    std::string_view output_flag;
    bool has_env_action = false;
    bool has_db_action = false;
    bool has_db_install = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto arg = args[i];
        if (arg == "--format-only") {
            o.env.format_only = true;
            env_flag = arg;
            continue;
        }
        if (arg == "--dry-run") {
            o.collection.dry_run = true;
            gc_flag = arg;
            continue;
        }
        if (arg == "--timings") {
            o.timings = true;
            continue;
        }
        if (arg == "--skip-stale" || arg == "--files") {
            (arg == "--files" ? o.list_files : o.skip_stale) = true;
            output_flag = arg;
            continue;
        }
        if (arg == "--blame" || arg == "--blame=bisect" ||
            arg == "--no-blame") {
            o.blame = arg == "--no-blame" ? types::blame_mode::off
                : arg == "--blame" ? types::blame_mode::cheap
                                   : types::blame_mode::bisect;
            blame_flag = arg;
            continue;
        }
        if (arg == "--all-findings") {
            o.all_findings = true;
            continue;
        }
        if (arg == "--ratchet") {
            const bool has_base = i + 1 < args.size() &&
                !args[i + 1].starts_with("-") && !command_named(args[i + 1]);
            o.ratchet = has_base ? std::string(args[++i]) : std::string();
            continue;
        }
        if (constexpr std::string_view with_base = "--ratchet=";
            arg.starts_with(with_base)) {
            o.ratchet = std::string(arg.substr(with_base.size()));
            if (o.ratchet->empty())
                return usage_error{"--ratchet= needs a base"};
            continue;
        }
        if (!arg.starts_with("-")) {
            if (which == types::command::blame && o.target.empty()) {
                o.target = arg;
                continue;
            }
            if (which == types::command::env) {
                if (auto e = env_word(arg, has_env_action, o.env))
                    return *e;
                continue;
            }
            if (which == types::command::db) {
                if (auto e = db_word(arg, has_db_action, has_db_install,
                        o.db))
                    return *e;
                continue;
            }
            if (which)
                return usage_error{"unexpected argument: " + std::string(arg)};
            which = command_named(arg);
            if (!which)
                return usage_error{"unknown command: " + std::string(arg)};
            continue;
        }
        if (!takes_value(arg))
            return usage_error{"unknown option: " + std::string(arg)};
        if (i + 1 == args.size())
            return usage_error{std::string(arg) + " needs a value"};
        const auto value = args[++i];
        const auto bad_value = [&] {
            return usage_error{"bad value for " + std::string(arg) + ": " +
                std::string(value)};
        };
        if (arg == "-C") {
            checkout = value;
        } else if (arg == "--meta") {
            o.command_line.meta = {value, command_line};
        } else if (arg == "--out") {
            o.command_line.output = {value, command_line};
        } else if (arg == "-j") {
            const auto n = parse_number<unsigned>(value);
            if (!n || *n == 0)
                return bad_value();
            o.threads = *n;
        } else if (arg == "--set") {
            if (!add_setting(value, o.command_line.settings))
                return bad_value();
        } else if (arg == "--codec") {
            if (value.empty() || value.starts_with("-"))
                return bad_value();
            o.only.emplace_back(value);
        } else if (arg == "--scope") {
            if (value.empty())
                return bad_value();
            o.scope.emplace_back(value);
            output_flag = arg;
        } else if (arg == "--commit") {
            if (value.empty() || value.starts_with("-"))
                return bad_value();
            o.revision = value;
        } else if (arg == "--bisect-from") {
            if (value.empty() || value.starts_with("-"))
                return bad_value();
            o.bisect_from = value;
        } else if (arg == "--input" || arg == "--var") {
            if (!add_pair(value, arg == "--input" ? o.env.inputs
                                                  : o.env.variables))
                return bad_value();
            env_flag = arg;
        } else if (arg == "--file") {
            if (value.empty())
                return bad_value();
            o.env.file = value;
            env_flag = arg;
        } else if (arg == "--adds" || arg == "--removes") {
            if (!add_names(value, arg == "--adds" ? o.env.adds
                                                  : o.env.removes))
                return bad_value();
            env_flag = arg;
        } else if (arg == "--environment" || arg == "--database" ||
            arg == "--text-search") {
            if (value.empty() || value.starts_with("-"))
                return bad_value();
            (arg == "--environment" ? o.environment
                : arg == "--database" ? o.db.database
                                      : o.db.text_search) = value;
        } else if (arg == "--today") {
            o.today = codec::ops::parse_date(value);
            if (!o.today)
                return bad_value();
        } else {
            const auto n = parse_number<std::uint64_t>(value);
            if (!n)
                return bad_value();
            if (arg == "--cap")
                o.collection.cap = *n;
            else
                o.collection.grace = std::chrono::seconds(*n);
            gc_flag = arg;
        }
    }
    if (!which)
        return usage_error{"no command"};
    if (const auto e = env_usage(*which, has_env_action, env_flag, o))
        return *e;
    if (!gc_flag.empty() && *which != types::command::gc)
        return usage_error{std::string(gc_flag) + " applies to gc only"};
    if (!output_flag.empty() && *which != types::command::generate &&
        *which != types::command::diff)
        return usage_error{std::string(output_flag) + " applies to generate "
            "and diff only"};
    if (const auto e = db_usage(*which, has_db_action, has_db_install, o))
        return *e;
    if (!o.revision.empty() && *which != types::command::check &&
        *which != types::command::sync)
        return usage_error{"--commit applies to check and sync only"};
    if (!o.only.empty() && *which != types::command::check)
        return usage_error{"--codec applies to check only"};
    if (o.ratchet && *which != types::command::check)
        return usage_error{"--ratchet applies to check only"};
    if (o.today && *which != types::command::check &&
        *which != types::command::hook && *which != types::command::blame &&
        !(*which == types::command::env &&
            o.env.action == types::env_action::version_new))
        return usage_error{"--today applies to check, hook, blame and env "
            "version new only"};
    if (o.all_findings && *which != types::command::hook)
        return usage_error{"--all-findings applies to hook only"};
    if (!blame_flag.empty() && *which != types::command::check &&
        !(*which == types::command::diff && blame_flag == "--blame"))
        return usage_error{std::string(blame_flag) + " applies to check, "
            "and --blame to diff, only"};
    if (*which == types::command::diff && blame_flag.empty())
        o.blame = types::blame_mode::off;
    if (!o.bisect_from.empty() && *which != types::command::check &&
        *which != types::command::blame)
        return usage_error{"--bisect-from applies to check and blame only"};
    if (*which == types::command::blame && o.target.empty())
        return usage_error{"blame needs a finding: <path>:<line>:<rule>"};
    if (o.ratchet && !o.revision.empty())
        return usage_error{"--ratchet and --commit cannot be combined"};
    o.which = *which;
    o.checkout = absolute_under(working_directory, checkout);
    return o;
}

}

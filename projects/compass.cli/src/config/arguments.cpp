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
    "  --commit <rev>              check: check this commit, read from git's "
    "objects, not the files on disk\n"
    "  --ratchet [<base>]          check: fail only on diagnostics the merge "
    "base of HEAD and <base> lacks (default: the upstream, else "
    "origin/main)\n"
    "  --today <YYYY-MM-DD>        check, hook: judge the expiry of exceptions "
    "as of this date (default: today in UTC)\n"
    "  --dry-run                   gc: remove nothing, report what would go\n"
    "  --cap <bytes>               gc: the size the store comes down to "
    "(default: 0)\n"
    "  --grace <seconds>           gc: never remove a file younger than this "
    "(default: 3600)\n"
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
        flag == "--grace" || flag == "--commit" || flag == "--today";
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
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto arg = args[i];
        if (arg == "--dry-run") {
            o.collection.dry_run = true;
            gc_flag = arg;
            continue;
        }
        if (arg == "--timings") {
            o.timings = true;
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
        } else if (arg == "--commit") {
            if (value.empty() || value.starts_with("-"))
                return bad_value();
            o.revision = value;
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
    if (!gc_flag.empty() && *which != types::command::gc)
        return usage_error{std::string(gc_flag) + " applies to gc only"};
    if (!o.revision.empty() && *which != types::command::check)
        return usage_error{"--commit applies to check only"};
    if (o.ratchet && *which != types::command::check)
        return usage_error{"--ratchet applies to check only"};
    if (o.today && *which != types::command::check &&
        *which != types::command::hook)
        return usage_error{"--today applies to check and hook only"};
    if (o.ratchet && !o.revision.empty())
        return usage_error{"--ratchet and --commit cannot be combined"};
    o.which = *which;
    o.checkout = absolute_under(working_directory, checkout);
    return o;
}

}

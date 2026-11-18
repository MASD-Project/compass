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
#include "compass.cli/app/application.hpp"

#include <chrono>
#include <exception>
#include <expected>
#include <iterator>
#include <string>
#include <thread>
#include <tuple>
#include <variant>
#include "compass.cli/app/pipeline.hpp"
#include "compass.cli/config/arguments.hpp"
#include "compass.cli/config/precedence.hpp"
#include "compass.cli/config/repository_file.hpp"
#include "compass.cli/io/hook_payload.hpp"
#include "compass.cli/io/listing.hpp"
#include "compass.cli/io/report.hpp"
#include "compass.cli/io/version.hpp"
#include "compass.platform/process/executable.hpp"
#include "compass.store/io/git_objects.hpp"
#include "compass.store/io/location.hpp"
#include "compass.cli/app/codecs.hpp"

namespace compass::cli::app {

namespace {

int run_generate(const types::options& o, bool timings, std::ostream& out,
    std::ostream& err) {
    const auto r = generate(o);
    io::write_generate(out, err, r);
    if (timings)
        io::write_timings(err, r.run);
    return r.run.failed() || !r.sink.problems.empty() ? exit_failure
                                                     : exit_success;
}

int run_diff(const types::options& o, bool timings, std::ostream& out,
    std::ostream& err) {
    const auto r = diff(o);
    io::write_diff(out, err, r);
    if (timings)
        io::write_timings(err, r.run);
    if (r.run.failed() || !r.planned.problems.empty())
        return exit_failure;
    const auto& p = r.planned;
    return p.written + p.restored + p.removed != 0 ? exit_findings
                                                   : exit_success;
}

int run_check(const types::options& o, bool timings, std::ostream& out,
    std::ostream& err) {
    const auto r = check(o);
    io::write_check(out, err, r);
    if (timings)
        io::write_timings(err, r.run);
    if (r.run.failed())
        return exit_failure;
    return r.findings.empty() ? exit_success : exit_findings;
}

int run_ratchet(const types::options& o, std::string_view base,
    bool timings, std::ostream& out, std::ostream& err) {
    const auto r = ratchet(o, base);
    io::write_ratchet(out, err, r);
    if (timings) {
        io::write_timings(err, r.current.run);
        err << "compass: merge base:\n";
        io::write_timings(err, r.previous.run);
    }
    if (r.failed())
        return exit_failure;
    return r.split.fresh.empty() ? exit_success : exit_findings;
}

/**
 * @brief The layer of the configuration file in effect for @p i: the one
 * at its commit, or the checkout's when there is no commit or the commit
 * has none, which @p warnings then says.
 */
std::expected<types::layer, std::string> repository_layer(
    const types::invocation& i,
    const std::optional<store::types::object_id>& commit,
    std::vector<std::string>& warnings) {
    if (commit) {
        auto at_commit = config::read_repository_file(store::io::git_objects(
            store::io::git_objects_dir(
                store::io::git_common_dir(i.checkout))), *commit);
        if (!at_commit)
            return std::unexpected(at_commit.error());
        if (*at_commit)
            return std::move(**at_commit);
        warnings.push_back("the commit " + i.revision + " has no " +
            std::string(config::repository_file_name) + "; using the "
            "working tree's");
    }
    return config::read_repository_file(i.checkout);
}

/**
 * @brief The settings and paths in effect for the checkout of @p i: its
 * configuration file, or the one at its commit, then the command line.
 * @return the problems, when either cannot be read or applied.
 */
std::expected<types::options, std::vector<std::string>> options_of(
    const types::invocation& i, types::configuration& c) {
    std::optional<store::types::object_id> commit;
    if (!i.revision.empty())
        commit = io::resolve_commit(i.checkout, i.revision);
    std::vector<std::string> warnings;
    const auto file = repository_layer(i, commit, warnings);
    if (!file)
        return std::unexpected(std::vector<std::string>{file.error()});
    c = config::resolve(codecs(), i.checkout,
        i.checkout / config::repository_file_name, *file, i.command_line);
    c.warnings = std::move(warnings);
    if (!c.problems.empty())
        return std::unexpected(c.problems);
    return types::options{.which = i.which,
        .checkout = i.checkout,
        .commit = commit,
        .meta = c.meta.value,
        .output_root = c.output.value,
        .threads = i.threads,
        .today = i.today.value_or(std::chrono::year_month_day{
            std::chrono::floor<std::chrono::days>(
                std::chrono::system_clock::now())}),
        .settings = config::settings_of(c),
        .exceptions = file->exceptions,
        .collection = i.collection,
        .all_findings = i.all_findings};
}

/**
 * @brief Checks the file the PostToolUse payload on @p in names, in the
 * checkout that holds it, and always succeeds.
 */
int run_hook(types::invocation i, std::istream& in, std::ostream& out,
    std::ostream& err) {
    types::hook_result r;
    try {
        const std::string payload(std::istreambuf_iterator<char>(in), {});
        const auto p = io::parse_hook_payload(payload);
        if (!p) {
            r.note = p.error();
            io::write_hook(out, err, r);
            return exit_success;
        }
        const auto checkout = io::checkout_of(p->file);
        if (!checkout)
            return exit_success;
        i.checkout = *checkout;
        types::configuration c;
        const auto o = options_of(i, c);
        if (!o) {
            r.note = o.error().front();
            io::write_hook(out, err, r);
            return exit_success;
        }
        r = hook(*o, p->file.lexically_relative(*checkout).generic_string(),
            p->created);
        io::write_hook(out, err, r);
        if (i.timings)
            io::write_timings(err, r.run);
    } catch (const std::exception& e) {
        r.findings.clear();
        r.note = std::string("failed (") + e.what() + "); nothing was checked";
        io::write_hook(out, err, r);
    }
    return exit_success;
}

int run_command(const types::invocation& i, std::ostream& out,
    std::ostream& err) {
    types::configuration c;
    const auto resolved = options_of(i, c);
    if (!resolved) {
        for (const auto& p : resolved.error())
            err << "compass: " << p << "\n";
        return exit_failure;
    }
    for (const auto& w : c.warnings)
        err << "compass: warning: " << w << "\n";
    const auto& o = *resolved;
    switch (o.which) {
    case types::command::generate:
        return run_generate(o, i.timings, out, err);
    case types::command::diff: return run_diff(o, i.timings, out, err);
    case types::command::check:
        if (i.ratchet)
            return run_ratchet(o, *i.ratchet, i.timings, out, err);
        return run_check(o, i.timings, out, err);
    case types::command::gc:
        io::write_gc(out, gc(o));
        return exit_success;
    case types::command::config:
        io::write_configuration(out, c, o.checkout);
        return exit_success;
    case types::command::hook:
        break;
    }
    return exit_failure;
}

}

int run(std::span<const std::string_view> args,
    const std::filesystem::path& working_directory, std::istream& in,
    std::ostream& out, std::ostream& err) {
    const auto parsed = config::parse_arguments(args, working_directory,
        std::thread::hardware_concurrency());
    if (std::holds_alternative<config::show_help>(parsed)) {
        out << config::help_text();
        return exit_success;
    }
    if (std::holds_alternative<config::show_version>(parsed)) {
        out << io::version_text(platform::process::executable_path());
        return exit_success;
    }
    if (const auto* e = std::get_if<config::usage_error>(&parsed)) {
        err << "compass: " << e->message << "\n\n" << config::help_text();
        return exit_failure;
    }
    const auto& invocation = std::get<types::invocation>(parsed);
    if (invocation.which == types::command::hook)
        return run_hook(invocation, in, out, err);
    try {
        return run_command(invocation, out, err);
    } catch (const std::exception& e) {
        err << "compass: " << e.what() << "\n";
        return exit_failure;
    }
}

}

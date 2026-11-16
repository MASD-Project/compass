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

#include <exception>
#include <thread>
#include <variant>
#include "compass.cli/app/pipeline.hpp"
#include "compass.cli/config/arguments.hpp"
#include "compass.cli/io/report.hpp"

namespace compass::cli::app {

namespace {

int run_generate(const types::options& o, std::ostream& out,
    std::ostream& err) {
    const auto r = generate(o);
    io::write_generate(out, err, r);
    return r.run.failed() || !r.sink.problems.empty() ? exit_failure
                                                     : exit_success;
}

int run_diff(const types::options& o, std::ostream& out, std::ostream& err) {
    const auto r = diff(o);
    io::write_diff(out, err, r);
    if (r.run.failed() || !r.planned.problems.empty())
        return exit_failure;
    const auto& p = r.planned;
    return p.written + p.restored + p.removed != 0 ? exit_findings
                                                   : exit_success;
}

int run_check(const types::options& o, std::ostream& out,
    std::ostream& err) {
    const auto r = check(o);
    io::write_check(out, err, r);
    if (r.run.failed())
        return exit_failure;
    return r.findings.empty() ? exit_success : exit_findings;
}

int run_command(const types::options& o, std::ostream& out,
    std::ostream& err) {
    switch (o.which) {
    case types::command::generate: return run_generate(o, out, err);
    case types::command::diff: return run_diff(o, out, err);
    case types::command::check: return run_check(o, out, err);
    case types::command::gc:
        io::write_gc(out, gc(o));
        return exit_success;
    }
    return exit_failure;
}

}

int run(std::span<const std::string_view> args,
    const std::filesystem::path& working_directory, std::ostream& out,
    std::ostream& err) {
    const auto parsed = config::parse_arguments(args, working_directory,
        std::thread::hardware_concurrency());
    if (std::holds_alternative<config::show_help>(parsed)) {
        out << config::help_text();
        return exit_success;
    }
    if (std::holds_alternative<config::show_version>(parsed)) {
        out << config::version_text();
        return exit_success;
    }
    if (const auto* e = std::get_if<config::usage_error>(&parsed)) {
        err << "compass: " << e->message << "\n\n" << config::help_text();
        return exit_failure;
    }
    try {
        return run_command(std::get<types::options>(parsed), out, err);
    } catch (const std::exception& e) {
        err << "compass: " << e.what() << "\n";
        return exit_failure;
    }
}

}

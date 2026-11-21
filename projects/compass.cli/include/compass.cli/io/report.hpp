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
#ifndef COMPASS_CLI_IO_REPORT_HPP
#define COMPASS_CLI_IO_REPORT_HPP

#include <filesystem>
#include <ostream>
#include "compass.cli/types/configuration.hpp"
#include "compass.cli/types/results.hpp"

namespace compass::cli::io {

/**
 * @brief Writes why the run failed to @p err, one line per load error and
 * registry problem.
 */
void write_problems(std::ostream& err, const types::run_summary& s);

/**
 * @brief Writes the sink problems to @p err and the counts of what
 * generate did to @p out, in one line.
 */
void write_generate(std::ostream& out, std::ostream& err,
    const types::generate_result& r);

/**
 * @brief Writes the unified diffs to @p out, so they can be piped, and
 * the problems and counts of what generate would do to @p err.
 */
void write_diff(std::ostream& out, std::ostream& err,
    const types::diff_result& r);

/**
 * @brief Writes each finding to @p out as
 * path:line:column: rule: message (fix: fix), what blaming them found, as
 * write_blame writes it, then their count; the problems go to @p err.
 */
void write_check(std::ostream& out, std::ostream& err,
    const types::check_result& r);

/**
 * @brief Writes one line of what checking the provenance of a branch
 * did: how many commits it checked and whether it read a pull request,
 * or why not; nothing when commit_provenance is off. Its findings are
 * written with the others.
 */
void write_provenance(std::ostream& out, const types::provenance_result& r);

/**
 * @brief Writes what blaming findings found to @p out, for the agent that
 * reads it: a line that counts them, then, for each environment the
 * findings already on main trace to, one message to pass on as it is,
 * naming the environment, or, when the commits state none, the author and
 * pull request of each, and listing each finding with its commit, pull
 * request, trailers, the method that found it and its fix. A guess, any
 * method but line, says "likely". The findings blame could not trace
 * follow, with why. Findings whose commits main does not hold are the
 * branch's own: counted, and listed only when @p all. Nothing when
 * nothing was blamed.
 */
void write_blame(std::ostream& out, const types::blame_report& r,
    bool all = false);

/**
 * @brief Writes each new finding of @p r to @p out as write_check does,
 * the provenance line, what blaming the inherited findings found, then
 * their count, how many were excused and the
 * merge base; the problems of both checks go to @p err, those of the
 * merge base marked.
 */
void write_ratchet(std::ostream& out, std::ostream& err,
    const types::ratchet_result& r);

/**
 * @brief Writes each path and each codec setting of @p c to @p out, one
 * per line, as "<key> = <value> (<source>)". The source is default, the
 * file and line, or command line. A path under @p checkout is written
 * relative to it.
 */
void write_configuration(std::ostream& out, const types::configuration& c,
    const std::filesystem::path& checkout);

/**
 * @brief Writes what Claude Code reads from a PostToolUse hook to @p out,
 * as one JSON object, or nothing when there is nothing to say.
 * With a note: {"hookSpecificOutput": {"hookEventName": "PostToolUse",
 * "additionalContext": "compass hook: <note>"}}, which adds the note to
 * the agent's context and blocks nothing. With findings:
 * {"decision": "block", "reason": "<count and file>\n<findings>"}, which
 * shows the agent each finding as write_check writes it, next to the
 * result of the edit. The problems of the run go to @p err.
 */
void write_hook(std::ostream& out, std::ostream& err,
    const types::hook_result& r);

/**
 * @brief Writes to @p err the time of each step of @p s, one per line,
 * then, per codec, how many units it ran, took from the record and took
 * from the result cache, and what formatting its files did.
 */
void write_timings(std::ostream& err, const types::run_summary& s);

/**
 * @brief Writes what gc removed, or would remove, to @p out.
 */
void write_gc(std::ostream& out, const types::gc_result& r);

}

#endif

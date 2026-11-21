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
#ifndef COMPASS_CLI_OPS_BLAME_HPP
#define COMPASS_CLI_OPS_BLAME_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.cli/types/provenance.hpp"
#include "compass.cli/types/results.hpp"
#include "compass.codec/types/descriptor.hpp"

/**
 * What blame reads from git's answers and how it groups what it found,
 * with no git and no files.
 */
namespace compass::cli::ops {

/**
 * @brief The name of @p m as the command line and the output write it:
 * line, added, drift or bisect.
 */
[[nodiscard]] std::string_view method_name(
    codec::types::blame_method m) noexcept;

/**
 * @brief What git blame --porcelain printed: the commit of each line, by
 * its 1-based line in the file, and the committer time of each commit.
 */
struct blamed_lines {
    std::map<std::uint32_t, std::string> commits;
    std::map<std::string, std::int64_t> times;
};

/**
 * @brief The commits and times of the lines git blame --porcelain
 * printed as @p lines.
 */
[[nodiscard]] blamed_lines parse_blame_porcelain(
    std::span<const std::string> lines);

/**
 * @brief The trailers of the commit message @p message, read leniently:
 * those of its last paragraph, as trailers_of reads them, then of each
 * paragraph before it that is all trailers too, so a block of
 * Story-ID, Task-ID and Environment set apart from a closing
 * Co-Authored-By by a blank line still counts. git reads only the last.
 */
[[nodiscard]] std::vector<types::trailer> closing_trailers(
    std::string_view message);

/**
 * @brief The pull request a commit subject names: n of
 * "Merge pull request #n from ...", which a merge on GitHub writes, or of
 * a trailing "(#n)", which a squash merge writes.
 */
[[nodiscard]] std::optional<std::uint64_t> pull_request_in(
    std::string_view subject);

/**
 * @brief The 1-based lines of @p file, first and last, that differ from
 * @p generated: those between the lines both start with and the lines
 * both end with. When @p file only lacks lines, the line where they
 * belong.
 */
[[nodiscard]] std::pair<std::uint32_t, std::uint32_t> differing_lines(
    std::string_view file, std::string_view generated);

/**
 * @brief A finding as compass blame names it: <path>:<line>:<rule>.
 */
struct finding_target {
    std::string path;
    std::uint32_t line = 0;
    std::string rule;
};

/**
 * @brief The finding @p spec names, or nothing when it is not of the
 * form <path>:<line>:<codec>/<rule>.
 */
[[nodiscard]] std::optional<finding_target> parse_target(
    std::string_view spec);

/**
 * @brief The findings blame traced to a commit main holds, whose most
 * likely culprit states one environment, or none when environment is
 * empty.
 */
struct notice {
    std::string environment;
    std::vector<const types::blamed_finding*> findings;
};

/**
 * @brief The findings of @p r whose most likely culprit main holds,
 * grouped by its environment, in name order, those that state none last;
 * one notice per environment, to be passed on as it is.
 */
[[nodiscard]] std::vector<notice> notices_of(const types::blame_report& r);

}

#endif

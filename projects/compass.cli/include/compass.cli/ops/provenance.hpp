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
#ifndef COMPASS_CLI_OPS_PROVENANCE_HPP
#define COMPASS_CLI_OPS_PROVENANCE_HPP

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.cli/types/provenance.hpp"
#include "compass.cli/types/results.hpp"
#include "compass.codec/types/descriptor.hpp"

/**
 * Commit provenance: whether each commit of a branch, and the branch's
 * pull request, state the work they belong to, as trailers and as rows of
 * the pull request's table, with values that name documents of the
 * checkout. What is asked is the project's: the tables of its check
 * document for the commit_provenance codec.
 */
namespace compass::cli::ops {

/**
 * @brief The name of the codec that holds the settings and the rules of
 * commit provenance.
 */
inline constexpr std::string_view provenance_codec = "commit_provenance";

/**
 * @brief The rules commit provenance reports under, in the order the
 * codec declares them.
 */
[[nodiscard]] std::span<const codec::types::rule> provenance_rule_list()
    noexcept;

/**
 * @brief The trailers of the commit message @p message, as git reads
 * them: the lines of its last paragraph, when that is not the subject's,
 * of the form "Key: value", a line that starts with a blank continuing
 * the one before. The paragraph holds trailers only when every line of it
 * is one or continues one.
 */
[[nodiscard]] std::vector<types::trailer> trailers_of(
    std::string_view message);

/**
 * @brief The rules of the check document whose tables @p data encodes, the
 * value of the codec's data option, with @p require, the value of its
 * require_pull_request option.
 * @return why they cannot be read, for the reader of the finding.
 */
[[nodiscard]] std::expected<types::provenance_rules, std::string>
provenance_rules_of(std::string_view data, std::string_view require);

/**
 * @brief One row of a Markdown table: its cells, trimmed, and the 1-based
 * line it is on.
 */
struct markdown_row {
    std::vector<std::string> cells;
    std::uint32_t line = 0;
};

/**
 * @brief The rows of the Markdown tables of @p body, separator rows left
 * out.
 */
[[nodiscard]] std::vector<markdown_row> markdown_rows(std::string_view body);

/**
 * @brief Checks @p commits, merges skipped, against @p rules: each carries
 * every trailer, unless an exemption lets it omit one, with a well-formed
 * value that names a document of @p documents of the right type when the
 * trailer names one. When @p pr is given its table must state each
 * trailer the rules label, likewise, and a commit whose value differs
 * from the pull request's is a warning, since a branch may touch a second
 * task. When it is not, @p no_pr says why, and the run fails for it only
 * when the rules require a pull request; @p branch names the branch then.
 */
[[nodiscard]] types::provenance_result check_provenance(
    const types::provenance_rules& rules,
    std::span<const types::branch_commit> commits,
    const types::document_ids& documents,
    const std::optional<types::pull_request>& pr, std::string_view no_pr,
    std::string_view branch);

}

#endif

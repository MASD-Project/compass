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
#ifndef COMPASS_CLI_APP_BLAME_HPP
#define COMPASS_CLI_APP_BLAME_HPP

#include <span>
#include "compass.cli/types/options.hpp"
#include "compass.cli/types/provenance.hpp"
#include "compass.cli/types/results.hpp"
#include "compass.codec/types/dependency_record.hpp"

namespace compass::cli::app {

/**
 * @brief The method compass blame traces a finding of the rule @p rule,
 * written <codec>/<rule>, with, as the descriptor of its codec, or the
 * check of @p o that defines it, declares: bisect for a rule nobody
 * declares.
 */
[[nodiscard]] codec::types::blame_method blame_method_of(
    const types::options& o, std::string_view rule);

/**
 * @brief Traces each of @p findings, found in the checkout of @p o, to the
 * commit that likely caused it, by the method its rule declares, and
 * reads that commit's trailers, pull request and whether main holds it.
 *
 * line runs one git blame per file, over the working tree. added follows
 * the finding's subject, or its file when it names none: git log -S on
 * the subject text, then the commit that added the subject. drift reads
 * the output from @p record, the record of the run that found it, blames
 * the lines of the file that differ from it, and finds the first commit
 * since the file last changed to an input the record says produced it.
 * bisect, which runs only when @p o asks for it, checks main's first-
 * parent merges at their commits with only the reporting codec, through
 * the result cache, to find the first merge with the finding, matched by
 * rule, path and message, then the commit of the merged branch that
 * brought it. A method that finds nothing falls back to bisect, when it
 * may run, and the note says so.
 *
 * Main is the upstream of the branch, else origin/main, else the branch
 * main, else HEAD. The trailers are those the commit_provenance check
 * document names, the titles of the documents they name read from
 * @p documents, and the environment the value of the trailer
 * commit_provenance.notify names, or of the first trailer with a
 * pattern. The pull request is the one the subject of the commit names,
 * else of the oldest first-parent merge of main that holds it. Nothing
 * is written but the store objects and result cache bisect's checks keep.
 */
[[nodiscard]] types::blame_report blame(const types::options& o,
    std::span<const types::finding> findings,
    const types::document_ids& documents,
    const codec::types::dependency_record& record);

}

#endif

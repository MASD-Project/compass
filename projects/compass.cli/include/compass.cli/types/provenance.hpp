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
#ifndef COMPASS_CLI_TYPES_PROVENANCE_HPP
#define COMPASS_CLI_TYPES_PROVENANCE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace compass::cli::types {

/**
 * @brief One trailer every commit of a branch carries: its key, as in
 * Story-ID. When type is not empty the value is the :ID: of a document of
 * that #+type:, an uppercase UUID; otherwise it matches pattern, whole.
 * label is the first cell of the row of the pull request's table that
 * states the same value, in its column, counted from 1 for the label's
 * own cell; no row is read when label is empty. line is the row of the
 * check document that states the trailer.
 */
struct trailer_rule {
    std::string trailer;
    std::string type;
    std::string pattern;
    std::string label;
    std::size_t column = 2;
    std::uint32_t line = 0;
};

/**
 * @brief Commits whose subject the pattern subject finds may omit the
 * trailers of omits; a trailer they carry is still checked.
 */
struct exemption {
    std::string subject;
    std::vector<std::string> omits;
};

/**
 * @brief What a project asks of the commits of a branch and of its pull
 * request: the trailers, the exemptions, the check document that states
 * them, and whether a branch with no pull request compass can read fails.
 */
struct provenance_rules {
    std::string source;
    std::vector<trailer_rule> trailers;
    std::vector<exemption> exemptions;
    bool require_pull_request = false;
};

/**
 * @brief One trailer of a commit message, with the 1-based line of the
 * message it is on.
 */
struct trailer {
    std::string key;
    std::string value;
    std::uint32_t line = 0;
};

/**
 * @brief One commit of a branch: its ID in hex, how many parents it has,
 * its subject and the trailers of its message.
 */
struct branch_commit {
    std::string id;
    std::size_t parents = 1;
    std::string subject;
    std::vector<trailer> trailers;
};

/**
 * @brief One commit, as blame reads it: its ID in hex, its author's name,
 * its subject and the trailers of its message.
 */
struct commit_detail {
    std::string id;
    std::string author;
    std::string subject;
    std::vector<trailer> trailers;
};

/**
 * @brief The pull request of a branch, as gh reports it.
 */
struct pull_request {
    std::uint64_t number = 0;
    std::string url;
    std::string body;
};

/**
 * @brief A document of the checkout, by its :ID:: its path, its #+type:
 * and its #+title:, each empty when it has none.
 */
struct document_ref {
    std::string path;
    std::string type;
    std::string title;
};

using document_ids = std::unordered_map<std::string, document_ref>;

}

#endif

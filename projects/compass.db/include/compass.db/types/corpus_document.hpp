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
#ifndef COMPASS_DB_TYPES_CORPUS_DOCUMENT_HPP
#define COMPASS_DB_TYPES_CORPUS_DOCUMENT_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.store/types/blob_id.hpp"

namespace compass::db::types {

/**
 * @brief A value of a corpus column that may be SQL NULL.
 */
using nullable = std::optional<std::string>;

/**
 * @brief The row of corpus.document: the common metadata as columns, every
 * keyword and the document's own properties as JSON objects, and the
 * source, which the caller keeps alive.
 *
 * keywords maps each keyword name, in lower case, to the list of its
 * values in order, so a repeated keyword keeps every value. properties
 * maps each property of the drawer before the first heading, in upper
 * case, to its value.
 */
struct document_row {
    nullable org_id;
    nullable title;
    nullable description;
    nullable type;
    nullable level;
    nullable audience;
    nullable created;
    nullable updated;
    std::vector<std::string> tags;
    std::string keywords;
    std::string properties;
    std::string_view source;
};

/**
 * @brief A row of corpus.heading. title is the title as plain text, a
 * link replaced by its description; path is the titles from the
 * outermost heading down to this one. anchor is the id compass.html gives
 * the heading, so a search hit lands where the rendered page puts it.
 */
struct heading_row {
    std::uint32_t level = 0;
    std::string title;
    std::vector<std::string> path;
    std::string anchor;
    nullable org_id;
    nullable todo;
    nullable priority;
    std::vector<std::string> tags;
    std::string properties;
};

/**
 * @brief A row of corpus.section: the text a heading holds before its
 * first subheading, or before the first heading for the preamble, as
 * plain text. heading is the ord of its heading, none for the preamble.
 */
struct section_row {
    std::optional<std::uint32_t> heading;
    std::string text;
};

/**
 * @brief A row of corpus.link. heading is the ord of the heading the link
 * sits under, none before the first heading. path is the link's path as
 * written, target_id the path of an id: link, and text its description,
 * or its path when it has none.
 */
struct link_row {
    std::optional<std::uint32_t> heading;
    std::string type;
    std::string path;
    nullable target_id;
    std::string text;
};

/**
 * @brief A row of test.step: a heading under * Steps, and its text but
 * the heading line and any Result subheading.
 */
struct step_row {
    std::string title;
    std::string body;
};

/**
 * @brief The row of test.scenario and its steps, for a document of
 * #+type: test_scenario. definition is the blob ID of the page with every
 * Result or Results heading and the State row of Scenario Info left out,
 * so it changes when the steps change and not when a result is written.
 */
struct scenario_row {
    nullable org_id;
    nullable title;
    nullable task_id;
    nullable story_id;
    nullable target;
    nullable environment;
    store::types::blob_id definition;
    std::vector<step_row> steps;
};

/**
 * @brief Every corpus row of one document, keyed by the blob ID of its
 * source. Headings, sections, links and steps are numbered by their place
 * in their vector, which is the ord of their row.
 */
struct corpus_document {
    store::types::blob_id hash;
    document_row document;
    std::vector<heading_row> headings;
    std::vector<section_row> sections;
    std::vector<link_row> links;
    std::optional<scenario_row> scenario;
};

}

#endif

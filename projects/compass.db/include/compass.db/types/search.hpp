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
#ifndef COMPASS_DB_TYPES_SEARCH_HPP
#define COMPASS_DB_TYPES_SEARCH_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace compass::db::types {

/**
 * @brief The caller's view: a SQL condition over the columns of
 * corpus.document, aliased d, compiled from the project's configuration.
 * Every query applies it before any other filter. It is trusted SQL, never
 * text a caller typed; "true" sees every document.
 */
struct view_filter {
    std::string condition;
};

/**
 * @brief A keyword or property a document must hold with this value.
 */
struct metadata_match {
    std::string name;
    std::string value;
};

/**
 * @brief The filters every search and facet count takes; each one set
 * must hold.
 *
 * A document has one type, state, level and audience. It must carry every
 * tag listed and match every keyword and property listed; a keyword
 * matches when any of its values is the one given. Keyword names are lower
 * case, property names as the drawer writes them. state is the State row
 * of the document's Status table. sprint is the :ID: of a sprint page: it
 * holds the page itself and every document whose preamble links to it, or
 * to a story whose preamble links to it.
 */
struct document_filter {
    std::optional<std::string> type;
    std::vector<std::string> tags;
    std::optional<std::string> state;
    std::optional<std::string> sprint;
    std::optional<std::string> level;
    std::optional<std::string> audience;
    std::vector<metadata_match> keywords;
    std::vector<metadata_match> properties;
};

/**
 * @brief Sections of an environment's documents matching a web-style
 * query, best first, inside the caller's view. q is in
 * websearch_to_tsquery's syntax: words, "phrases", or, -exclusions.
 */
struct search_request {
    std::string environment;
    std::string q;
    document_filter filter;
    std::uint32_t limit = 20;
    std::uint32_t offset = 0;
};

/**
 * @brief One matching section: its document's metadata and path in the
 * environment, the heading path and anchor of its heading, both empty for
 * the text before the first heading, and a snippet with each match
 * between <b> and </b>. rank is ts_rank_cd's score.
 */
struct search_hit {
    std::string org_id;
    std::string title;
    std::string type;
    std::string description;
    std::string path;
    std::vector<std::string> heading_path;
    std::string anchor;
    std::string snippet;
    double rank = 0;
};

/**
 * @brief A page of hits, how many sections match in all, and when the
 * environment was last synced.
 */
struct search_response {
    std::vector<search_hit> hits;
    std::uint64_t total = 0;
    std::string synced_at;
};

/**
 * @brief How hits are ordered: ts_rank_cd's weights of D, C, B and A, its
 * normalization flags, and the document types in the order that breaks a
 * tie of rank, types not listed after those listed.
 *
 * The defaults are the best measured against the recorded questions over
 * ORE Studio: normalization 2 divides a section's rank by its length, so a
 * long section that mentions every word once stops outranking the short
 * one about them.
 */
struct ranking {
    std::array<float, 4> weights{0.1F, 0.2F, 0.4F, 1.0F};
    std::uint32_t normalization = 2;
    std::vector<std::string> type_order;
};

/**
 * @brief The values of each filter over the documents a query matches
 * inside the caller's view and filters; every document of the environment
 * when q is empty. keywords and properties name the ones to count.
 */
struct facets_request {
    std::string environment;
    std::string q;
    document_filter filter;
    std::vector<std::string> keywords;
    std::vector<std::string> properties;
};

/**
 * @brief How many matching documents hold a value; label is the title of
 * a sprint, empty otherwise.
 */
struct facet_count {
    std::string value;
    std::string label;
    std::uint64_t count = 0;
};

/**
 * @brief The counts of the values of one keyword or property.
 */
struct named_facet {
    std::string name;
    std::vector<facet_count> counts;
};

/**
 * @brief The counts of each filter's values, most frequent first, and how
 * many documents match. A document placed at two paths counts twice.
 */
struct facets_response {
    std::uint64_t documents = 0;
    std::vector<facet_count> types;
    std::vector<facet_count> tags;
    std::vector<facet_count> states;
    std::vector<facet_count> sprints;
    std::vector<facet_count> levels;
    std::vector<facet_count> audiences;
    std::vector<named_facet> keywords;
    std::vector<named_facet> properties;
};

}

#endif

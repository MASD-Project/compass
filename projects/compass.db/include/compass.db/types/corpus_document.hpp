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

#include <optional>
#include <vector>
#include "compass.db/types/document_row.hpp"
#include "compass.db/types/heading_row.hpp"
#include "compass.db/types/link_row.hpp"
#include "compass.db/types/scenario_row.hpp"
#include "compass.db/types/section_row.hpp"
#include "compass.db/types/step_row.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::db::types {

/**
 * @brief The row of test.scenario and its steps, for a document of
 * #+type: test_scenario. definition is the blob ID of the page with every
 * Result or Results heading and the State row of Scenario Info left out,
 * so it changes when the steps change and not when a result is written.
 */
struct test_scenario : scenario_row {
    std::vector<step_row> steps;
};

/**
 * @brief Every corpus row of one document, keyed by the blob ID of its
 * source. Headings, sections, links and steps are numbered by their place
 * in their vector, which is the ord of their row.
 *
 * The rows are generated from the models of compass.db/modeling, which
 * say what each column holds; this aggregate, which is not a table, is
 * written by hand.
 */
struct corpus_document {
    store::types::blob_id hash;
    document_row document;
    std::vector<heading_row> headings;
    std::vector<section_row> sections;
    std::vector<link_row> links;
    std::optional<test_scenario> scenario;
};

}

#endif

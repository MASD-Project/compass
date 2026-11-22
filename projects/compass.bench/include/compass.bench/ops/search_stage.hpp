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
#ifndef COMPASS_BENCH_OPS_SEARCH_STAGE_HPP
#define COMPASS_BENCH_OPS_SEARCH_STAGE_HPP

#include <cstddef>
#include <filesystem>
#include <ostream>
#include <string>
#include <vector>
#include "compass.db/types/search.hpp"

namespace compass::bench::ops {

/**
 * @brief One recorded question: who asks it, the query as they type it,
 * and the :ID: of each page that answers it.
 */
struct question {
    std::string audience;
    std::string query;
    std::vector<std::string> answers;
};

/**
 * @brief How search did on one question. rank is the place of the first
 * answering page among the distinct documents of the hits, from 1; 0 when
 * none is among them. top holds the titles of the first three documents.
 * first_ms is the first run of the query for one page of the default size,
 * warm_ms each repetition after; the rank is read from a deeper page that
 * is not timed.
 */
struct question_result {
    question asked;
    std::size_t rank = 0;
    std::size_t total = 0;
    std::vector<std::string> top;
    double first_ms = 0;
    std::vector<double> warm_ms;
};

/**
 * @brief Reads a question set: one question a line, its audience, query
 * and space-separated answer :ID:s separated by tabs.
 */
[[nodiscard]] std::vector<question> read_questions(
    const std::filesystem::path& file);

/**
 * @brief Asks each question of the environment @p environment of the
 * database at @p conninfo, through the team view, ranked by @p how, once
 * and then @p repetitions times more.
 */
[[nodiscard]] std::vector<question_result> run_search_stage(
    const std::string& conninfo, const std::string& environment,
    const std::vector<question>& questions, const db::types::ranking& how,
    unsigned repetitions);

/**
 * @brief Each question's rank and top pages, then hit@1, hit@5 and MRR,
 * overall and by audience, and the median and p95 of the first and the
 * warm latencies.
 */
void write_search_report(std::ostream& os,
    const std::vector<question_result>& results);

}

#endif

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
#ifndef COMPASS_BENCH_OPS_TESTING_STAGE_HPP
#define COMPASS_BENCH_OPS_TESTING_STAGE_HPP

#include <ostream>
#include <string>
#include <vector>
#include "compass.db/types/testing.hpp"

namespace compass::bench::ops {

/**
 * @brief What needs testing in a sprint and its sprint state, through the
 * team view, with the latency of each question: the first run and each of
 * the repetitions after.
 */
struct testing_result {
    db::types::testing_response testing;
    db::types::sprint_state_response sprint;
    double testing_first_ms = 0;
    std::vector<double> testing_warm_ms;
    double sprint_first_ms = 0;
    std::vector<double> sprint_warm_ms;
};

/**
 * @brief Asks what needs testing and the sprint state of @p r of the
 * database at @p conninfo, once and then @p repetitions times more.
 */
[[nodiscard]] testing_result run_testing_stage(const std::string& conninfo,
    const db::types::sprint_request& r, unsigned repetitions);

/**
 * @brief The sprint, the count of scenarios in each state, each story with
 * its campaign, open notes, scenarios and tasks, and the median warm
 * latency of each question.
 */
void write_testing_report(std::ostream& os, const testing_result& result);

}

#endif

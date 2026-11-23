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
#ifndef COMPASS_BENCH_OPS_CAMPAIGN_STAGE_HPP
#define COMPASS_BENCH_OPS_CAMPAIGN_STAGE_HPP

#include <filesystem>
#include <istream>
#include <ostream>
#include <string>
#include "compass.db/types/campaign.hpp"

namespace compass::bench::ops {

/**
 * @brief Plays a script of campaign writes against the environment
 * @p environment of the database at @p conninfo, one write a line, and
 * reports each write's result, or why it was rejected, and its latency.
 *
 * A line is one of, words separated by spaces:
 *
 * - start <scenario :ID:> <tester>: starts a run, which the lines after
 *   act on, as that tester;
 * - campaign <id> <tester>: acts on that campaign, as that tester, with
 *   no run, so a note can be added to a campaign whose runs are done;
 * - step <ord> <pass|fail|blocked|skipped> [notes]: records a step result;
 * - attach <file> <ord|run|note> [caption]: attaches an image at a step of
 *   the run, to the run, or to the last note; a .jpg, .jpeg, .gif or .webp
 *   file is that type, any other a PNG;
 * - note <kind> <scenario :ID:|-> <ord|-> <text>: adds a note to the
 *   run's campaign;
 * - address <tester> [reply]: addresses the last note;
 * - finish: finishes the run.
 *
 * An empty line or one that starts with # is skipped. A tester of -
 * stands for none, so "start SC-1 -" is a write with no tester.
 */
void run_campaign_stage(const std::string& conninfo,
    const std::string& environment, const db::types::note_kinds& kinds,
    std::istream& script, const std::filesystem::path& base,
    std::ostream& os);

}

#endif

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
#ifndef COMPASS_DB_OPS_RUN_OUTCOME_HPP
#define COMPASS_DB_OPS_RUN_OUTCOME_HPP

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>
#include "compass.db/types/campaign.hpp"

namespace compass::db::ops {

/**
 * @brief The outcome of a run whose steps, in ord order, hold @p steps:
 * failed if any step failed; else blocked if any was blocked; else passed
 * when every step passed or was skipped. A run with a step that has no
 * result and none failed or blocked has no outcome yet: the ords of those
 * steps come back instead.
 */
[[nodiscard]] std::expected<types::run_outcome, std::vector<std::int32_t>>
derive_run_outcome(std::span<const std::optional<types::step_outcome>> steps);

}

#endif

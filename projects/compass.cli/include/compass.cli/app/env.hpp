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
#ifndef COMPASS_CLI_APP_ENV_HPP
#define COMPASS_CLI_APP_ENV_HPP

#include <ostream>
#include "compass.cli/types/options.hpp"

namespace compass::cli::app {

/**
 * @brief Runs compass env for @p i over its checkout, and returns the exit
 * status.
 *
 * The format is the check document of compass/checks whose Definition
 * names env_format. generate writes the file it describes: the old file,
 * kept as <file>.old, gives the secrets and kept values; the file is
 * created readable by its owner alone and replaces the old one in one
 * step. version prints the highest version; version new appends the next
 * one to the document, dated --today or today in UTC. status prints how
 * the file stands and exits with findings when it must be generated
 * again.
 */
[[nodiscard]] int run_env(const types::invocation& i, std::ostream& out,
    std::ostream& err);

}

#endif

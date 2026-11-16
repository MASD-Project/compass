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
#ifndef COMPASS_CLI_OPS_RATCHET_HPP
#define COMPASS_CLI_OPS_RATCHET_HPP

#include <span>
#include <string_view>
#include <vector>
#include "compass.cli/types/results.hpp"
#include "compass.store/types/git_object.hpp"

/**
 * Ratchet mode: which findings of a check are new against those of the
 * merge base, following the files HEAD renamed.
 */
namespace compass::cli::ops {

/**
 * @brief The renames among @p gone, paths @p base holds and @p head
 * lacks, that kept their content: each pairs a path of @p gone with a
 * path of @p head holding the same blob that @p base lacks. Paths sharing
 * a blob pair up in byte order. @p gone is sorted.
 */
[[nodiscard]] std::vector<types::rename> exact_renames(
    const store::types::commit_files& base,
    const store::types::commit_files& head,
    std::span<const std::string_view> gone);

/**
 * @brief Splits @p current against @p base, after moving each finding of
 * @p base at the from of one of @p renames to its to.
 *
 * A finding is matched by path, rule and message, not by line, and each
 * finding of @p base excuses at most one of @p current: a finding that
 * moved is not new, a second copy of one is. A drift finding is matched
 * by its sides instead of its message, so drift is excused only when
 * neither the file nor what is generated for it changed. Within a match,
 * a finding on the same line as an old one is excused first.
 */
[[nodiscard]] types::ratchet_split split_new(
    std::span<const types::finding> base,
    std::span<const types::finding> current,
    std::span<const types::rename> renames);

}

#endif

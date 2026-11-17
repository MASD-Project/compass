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
#ifndef COMPASS_CROSSCHECK_OPS_CSV_HPP
#define COMPASS_CROSSCHECK_OPS_CSV_HPP

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace compass::crosscheck::ops {

/**
 * @brief A text Python's csv module refuses: a line end inside an unquoted
 * field after a lone carriage return.
 */
class csv_error final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Python's csv.reader over @p text with the excel dialect and
 * @p delimiter, as a script feeds it: lines split at \n, each a record
 * unless a quoted field runs on.
 * @throws csv_error where the reader raises.
 */
[[nodiscard]] std::vector<std::vector<std::string>> csv_rows(
    std::string_view text, char delimiter);

}

#endif

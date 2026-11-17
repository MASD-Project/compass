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
#ifndef COMPASS_CPP_OPS_GLOB_HPP
#define COMPASS_CPP_OPS_GLOB_HPP

#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"

namespace compass::cpp::ops {

/**
 * @brief The files of the checkout @p pattern matches, as
 * sorted(root.glob(pattern)) lists them: pathlib's order, by component.
 * The view records one listing: the directories before the first wildcard
 * and the literal end of the pattern.
 */
[[nodiscard]] std::vector<std::string_view> glob(
    const compass::codec::types::corpus_view& v, std::string_view pattern);

}

#endif

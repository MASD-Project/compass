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
#ifndef COMPASS_CODEC_OPS_PAGES_HPP
#define COMPASS_CODEC_OPS_PAGES_HPP

#include <expected>
#include <string>
#include <string_view>
#include "compass.codec/types/corpus_view.hpp"

namespace compass::codec::ops {

/**
 * @brief The document whose :ID: is the value of the option @p key, or
 * why there is none, written for the reader of the finding: the option is
 * empty, or no document the codec reads has that ID.
 *
 * A codec names a page of its project this way so that the project can
 * move or rename the page: the lookup is an ID input, whose answer is the
 * page's path, so a unit that found the page runs again when it moves.
 */
[[nodiscard]] std::expected<types::document, std::string> page_of(
    const types::corpus_view& v, std::string_view key);

}

#endif

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
#ifndef COMPASS_CODEC_TYPES_CORPUS_INDEX_HPP
#define COMPASS_CODEC_TYPES_CORPUS_INDEX_HPP

#include <string_view>
#include <unordered_map>
#include "compass.org/types/handle.hpp"

namespace compass::codec::types {

/**
 * @brief The row of each document of a corpus by its path and by the ID
 * property of its file-level drawer.
 * When two documents carry one ID, the first in corpus order holds it. The
 * keys point into the corpus.
 */
struct corpus_index {
    std::unordered_map<std::string_view, compass::org::types::document_id>
        paths;
    std::unordered_map<std::string_view, compass::org::types::document_id>
        ids;
};

}

#endif

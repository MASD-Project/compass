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
#ifndef COMPASS_CODEC_TYPES_CORPUS_HPP
#define COMPASS_CODEC_TYPES_CORPUS_HPP

#include <cstddef>
#include <string_view>
#include <vector>
#include "compass.org/types/handle.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.org/types/type_registry.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::types {

/**
 * @brief The documents of one checkout in memory, one row per document,
 * indexed by document_id.
 *
 * paths are relative to the root of the checkout. contents holds the git
 * blob ID of each source, sources its bytes and tables its parse, whose
 * ranges are read against those bytes. types holds the registered type the
 * first #+type: keyword names, or none for a document with no such keyword
 * or with a value the registry does not hold.
 *
 * The corpus owns nothing: whoever loaded it keeps the paths, the sources
 * and the objects behind the tables alive and unchanged while a codec runs.
 */
struct corpus {
    std::vector<std::string_view> paths;
    std::vector<compass::store::types::blob_id> contents;
    std::vector<std::string_view> sources;
    std::vector<compass::org::types::tables_view> tables;
    std::vector<compass::org::types::type_id> types;

    [[nodiscard]] std::size_t size() const noexcept { return paths.size(); }
};

}

#endif

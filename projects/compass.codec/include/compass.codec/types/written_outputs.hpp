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
#ifndef COMPASS_CODEC_TYPES_WRITTEN_OUTPUTS_HPP
#define COMPASS_CODEC_TYPES_WRITTEN_OUTPUTS_HPP

#include <cstddef>
#include <string>
#include <vector>
#include "compass.store/types/blob_id.hpp"
#include "compass.store/types/git_index.hpp"

namespace compass::codec::types {

/**
 * @brief The files compass wrote under one output root, as parallel
 * columns sorted by path: the codec that produced each, the blob ID of
 * the bytes written and the stat data the file had after the write.
 *
 * root is the absolute output root the paths are relative to. written is
 * the time the run that last changed the record started. An entry whose
 * file was modified in that second or later is racily clean: an edit in
 * the same second could leave every stat field as it was, so the file is
 * compared by content instead.
 */
struct written_outputs {
    std::string root;
    compass::store::types::index_time written;
    std::vector<std::string> paths;
    std::vector<std::string> codecs;
    std::vector<compass::store::types::blob_id> hashes;
    std::vector<compass::store::types::stat_data> stats;

    [[nodiscard]] std::size_t size() const noexcept { return paths.size(); }
};

}

#endif

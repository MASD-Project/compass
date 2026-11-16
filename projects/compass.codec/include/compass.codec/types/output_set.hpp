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
#ifndef COMPASS_CODEC_TYPES_OUTPUT_SET_HPP
#define COMPASS_CODEC_TYPES_OUTPUT_SET_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/file_table.hpp"
#include "compass.codec/types/text_column.hpp"
#include "compass.org/types/column.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::types {

/**
 * @brief The files one run asks to have on disk, one row per file: the
 * codec that produced it, its path relative to the output root, its bytes
 * and their blob ID.
 *
 * complete names every codec whose outputs are all here: each of its units
 * ran or was reused with its recorded outputs. A file compass wrote for one
 * of these codecs and that is no longer here is removed. A codec missing
 * from complete, such as a disabled one, keeps the files it wrote before.
 */
struct output_set {
    std::vector<std::string> complete;
    text_column<file_id> codecs;
    file_table files;
    compass::org::types::column<file_id, compass::store::types::blob_id>
        hashes;

    [[nodiscard]] std::size_t size() const noexcept { return files.size(); }

    void add(std::string_view codec, std::string_view path,
        std::string_view content, const compass::store::types::blob_id& hash) {
        codecs.add(codec);
        files.add(path, content);
        hashes.values.push_back(hash);
    }
};

}

#endif

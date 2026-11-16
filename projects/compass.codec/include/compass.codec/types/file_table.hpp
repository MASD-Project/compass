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
#ifndef COMPASS_CODEC_TYPES_FILE_TABLE_HPP
#define COMPASS_CODEC_TYPES_FILE_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include "compass.codec/types/text_column.hpp"
#include "compass.org/types/handle.hpp"

namespace compass::codec::types {

using file_id = compass::org::types::handle<struct file_tag>;

/**
 * @brief The files a codec produced, one row per file.
 *
 * A path is relative to the root of the checkout. The codec states what
 * each file holds and never writes it: whoever ran the codec decides
 * whether the bytes reach the disk, a diff or a cache.
 */
struct file_table {
    text_column<file_id> paths;
    text_column<file_id, std::uint64_t> contents;

    [[nodiscard]] std::size_t size() const noexcept { return paths.size(); }

    void add(std::string_view path, std::string_view content) {
        paths.add(path);
        contents.add(content);
    }

    void append(const file_table& other) {
        paths.append(other.paths);
        contents.append(other.contents);
    }
};

}

#endif

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
#ifndef COMPASS_CODEC_TYPES_CHECKOUT_FILES_HPP
#define COMPASS_CODEC_TYPES_CHECKOUT_FILES_HPP

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.store/types/blob_id.hpp"

namespace compass::codec::types {

/**
 * @brief Every file of a checkout, documents or not, which a codec reads
 * through the file inputs of its view.
 * paths are relative to the root of the checkout, in byte order, each
 * once. content_of gives the content ID of one of them as the host
 * resolves IDs, without reading it when the host can, and read gives its
 * bytes; either gives nothing when the file cannot be read. Both are
 * called from many threads at once. A file that paths does not hold is
 * absent, whatever is on disk.
 * The host keeps the strings paths views alive while a codec runs.
 */
struct checkout_files {
    std::vector<std::string_view> paths;
    std::function<std::optional<compass::store::types::blob_id>(
        std::string_view)> content_of;
    std::function<std::optional<std::string>(std::string_view)> read;
};

}

#endif

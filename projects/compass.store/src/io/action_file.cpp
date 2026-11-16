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
#include "compass.store/io/action_file.hpp"

#include <fstream>
#include <iterator>
#include <span>
#include "compass.store/io/location.hpp"
#include "compass.store/io/object_file.hpp"
#include "compass.store/ops/action_object.hpp"

namespace compass::store::io {

std::optional<std::string> read_action(const std::filesystem::path& dir,
    const types::blob_id& key) {
    std::ifstream is(object_path(dir, key), std::ios::binary);
    if (!is)
        return std::nullopt;
    const std::string bytes(std::istreambuf_iterator<char>(is), {});
    const auto body = ops::decode_action(key, bytes);
    if (!body)
        return std::nullopt;
    return std::string(*body);
}

void write_action(const std::filesystem::path& dir,
    const types::blob_id& key, std::string_view body) {
    const auto bytes = ops::encode_action(key, body);
    write_object(object_path(dir, key), std::as_bytes(std::span(bytes)));
}

}

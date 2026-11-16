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
#include "compass.codec/io/dependency_file.hpp"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include "compass.codec/ops/dependencies.hpp"
#include "compass.store/io/object_file.hpp"

namespace compass::codec::io {

namespace {

std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream is(path, std::ios::binary);
    if (!is)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(is), {});
}

}

std::filesystem::path dependency_file_path(
    const std::filesystem::path& git_dir) {
    return git_dir / "compass" / "deps";
}

loaded_record read_dependency_file(const std::filesystem::path& path) {
    loaded_record r;
    const auto bytes = read_file(path);
    if (!bytes)
        return r;
    r.bytes = bytes->size();
    try {
        r.record = ops::decode_dependency_record(*bytes);
    } catch (const std::runtime_error& e) {
        r.problem = e.what();
    }
    return r;
}

bool write_dependency_file(const std::filesystem::path& path,
    const types::dependency_record& record) {
    const auto bytes = ops::encode_dependency_record(record);
    if (read_file(path) == bytes)
        return false;
    store::io::write_object(path, std::as_bytes(std::span(bytes)));
    return true;
}

}

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
#include "compass.codec/io/unmarked_file.hpp"

#include <algorithm>
#include <cstring>
#include <span>
#include <string>
#include "compass.codec/ops/exceptions.hpp"
#include "compass.store/io/object_file.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::codec::io {

namespace {

using compass::store::types::blob_id;

constexpr std::size_t id_size = sizeof(blob_id::bytes);

/**
 * @brief The first bytes of the file. Naming the marker makes a file
 * written for another marker unreadable rather than wrong.
 */
std::string header() {
    return "CMPSUNM\n" + std::string(ops::exception_marker) + "\n";
}

std::string encode(const std::vector<blob_id>& ids) {
    auto out = header();
    out.reserve(out.size() + (ids.size() + 1) * id_size);
    for (const auto& id : ids)
        out.append(reinterpret_cast<const char*>(id.bytes.data()), id_size);
    const auto digest = store::ops::sha1_of(out);
    out.append(reinterpret_cast<const char*>(digest.data()), id_size);
    return out;
}

}

std::filesystem::path unmarked_file_path(
    const std::filesystem::path& git_dir) {
    return git_dir / "compass" / "unmarked";
}

std::vector<blob_id> read_unmarked_file(const std::filesystem::path& path) {
    const auto bytes = store::io::read_file(path);
    const auto start = header();
    if (!bytes || !bytes->starts_with(start) ||
        bytes->size() < start.size() + id_size ||
        (bytes->size() - start.size()) % id_size != 0)
        return {};
    const std::string_view body(bytes->data(), bytes->size() - id_size);
    const auto digest = store::ops::sha1_of(body);
    if (std::memcmp(digest.data(), body.data() + body.size(), id_size) != 0)
        return {};
    std::vector<blob_id> ids((body.size() - start.size()) / id_size);
    for (std::size_t i = 0; i < ids.size(); ++i)
        std::memcpy(ids[i].bytes.data(),
            body.data() + start.size() + i * id_size, id_size);
    if (!std::ranges::is_sorted(ids))
        return {};
    return ids;
}

bool write_unmarked_file(const std::filesystem::path& path,
    const std::vector<blob_id>& ids) {
    auto sorted = ids;
    std::ranges::sort(sorted);
    const auto bytes = encode(sorted);
    if (store::io::read_file(path) == bytes)
        return false;
    store::io::write_object(path, std::as_bytes(std::span(bytes)));
    return true;
}

}

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
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include "compass.codec/ops/dependencies.hpp"
#include "compass.store/io/object_file.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::codec::io {

namespace {

/**
 * @brief The @p size bytes of @p in at @p offset.
 * @throws std::runtime_error when they cannot all be read.
 */
std::string read_at(std::ifstream& in, std::uint64_t offset,
    std::uint64_t size) {
    std::string bytes(size, '\0');
    in.seekg(static_cast<std::streamoff>(offset));
    in.read(bytes.data(), static_cast<std::streamsize>(size));
    if (!in || static_cast<std::uint64_t>(in.gcount()) != size)
        throw std::runtime_error("dependency record: truncated");
    return bytes;
}

}

std::filesystem::path dependency_file_path(
    const std::filesystem::path& git_dir) {
    return git_dir / "compass" / "deps";
}

loaded_record read_dependency_file(const std::filesystem::path& path) {
    loaded_record r;
    const auto bytes = store::io::read_file(path);
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

loaded_index read_unit_index(const std::filesystem::path& path) {
    loaded_index r;
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        r.missing = true;
        return r;
    }
    try {
        const auto size = static_cast<std::uint64_t>(in.tellg());
        if (size < ops::record_header_size + ops::record_footer_size)
            throw std::runtime_error("dependency record: truncated");
        const auto header = read_at(in, 0, ops::record_header_size);
        const auto layout = ops::locate_unit_index(header,
            read_at(in, size - ops::record_footer_size,
                ops::record_footer_size), size);
        r.index = ops::decode_unit_index(header,
            read_at(in, layout.index_offset, layout.index_size), layout);
    } catch (const std::runtime_error& e) {
        r.index = {};
        r.problem = e.what();
    }
    return r;
}

loaded_record read_record_units(const std::filesystem::path& path,
    const types::unit_index& index,
    std::span<const types::record_unit_id> units) {
    loaded_record r;
    std::ifstream in(path, std::ios::binary);
    try {
        if (!in)
            throw std::runtime_error("dependency record: cannot be opened");
        for (const auto u : units) {
            const auto begin = index.offsets[u.value];
            const auto size = index.offsets[u.value + 1] - begin;
            ops::decode_record_unit(read_at(in, begin, size),
                index.hashes[u], r.record);
            r.bytes += size;
        }
    } catch (const std::runtime_error& e) {
        r.record = {};
        r.problem = e.what();
    }
    return r;
}

std::optional<std::string> read_recorded_file(
    const std::filesystem::path& path, const types::unit_index& index,
    types::record_unit_id unit, std::uint32_t file) {
    std::ifstream in(path, std::ios::binary);
    try {
        if (!in)
            return std::nullopt;
        const auto u = unit.value;
        const auto head = read_at(in, index.offsets[u],
            index.offsets[u + 1] - index.offsets[u]);
        types::dependency_record scratch;
        const auto sizes = ops::decode_record_unit(head, index.hashes[unit],
            scratch);
        if (file >= sizes.size())
            return std::nullopt;
        std::uint64_t at = index.content_offsets[u];
        for (std::uint32_t f = 0; f < file; ++f)
            at += sizes[f];
        auto bytes = read_at(in, at, sizes[file]);
        if (store::ops::blob_id_of(bytes) !=
            scratch.file_hashes[types::file_id{file}])
            return std::nullopt;
        return bytes;
    } catch (const std::runtime_error&) {
        return std::nullopt;
    }
}

bool write_dependency_file(const std::filesystem::path& path,
    const types::dependency_record& record) {
    const auto bytes = ops::encode_dependency_record(record);
    if (store::io::read_file(path) == bytes)
        return false;
    store::io::write_object(path, std::as_bytes(std::span(bytes)));
    return true;
}

}

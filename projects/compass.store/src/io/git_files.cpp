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
#include "compass.store/io/git_files.hpp"

#include <string>
#include "compass.platform/filesystem/file_status.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/ops/git_index.hpp"

namespace compass::store::io {

namespace {

namespace fs = platform::filesystem;

types::index_time to_time(const fs::file_time& t) noexcept {
    return {static_cast<std::uint32_t>(t.seconds), t.nanoseconds};
}

}

types::git_index read_git_index(const std::filesystem::path& file) {
    std::string bytes;
    const auto status = fs::read_file(file, bytes);
    auto index = ops::parse_git_index(bytes);
    index.written = to_time(status.modified);
    return index;
}

std::optional<types::stat_data> stat_of(const std::filesystem::path& file) {
    const auto st = fs::regular_file_status(file);
    if (!st)
        return std::nullopt;
    types::stat_data s;
    s.ctime = to_time(st->changed);
    s.mtime = to_time(st->modified);
    s.dev = static_cast<std::uint32_t>(st->device);
    s.ino = static_cast<std::uint32_t>(st->inode);
    s.uid = st->user;
    s.gid = st->group;
    s.size = static_cast<std::uint32_t>(st->size);
    return s;
}

types::blob_id hash_file(const std::filesystem::path& file) {
    thread_local std::string bytes;
    fs::read_file(file, bytes);
    return ops::blob_id_of(bytes);
}

}

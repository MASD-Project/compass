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
#ifndef COMPASS_STORE_TYPES_GIT_OBJECT_HPP
#define COMPASS_STORE_TYPES_GIT_OBJECT_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.store/types/blob_id.hpp"

namespace compass::store::types {

/**
 * @brief The ID git gives any object: a blob, a tree, a commit or a tag.
 * A blob's object ID is its blob ID, the key of the store.
 */
using object_id = blob_id;

/**
 * @brief The type of a git object, numbered as pack files number them.
 */
enum class object_type : std::uint8_t {
    commit = 1, tree = 2, blob = 3, tag = 4
};

/**
 * @brief One object of git's object database: its type and its bytes,
 * without the "<type> <size>\0" header its ID is computed over.
 */
struct git_object {
    object_type type = object_type::blob;
    std::string bytes;
};

/**
 * @brief The modes of the tree entries that are not files, as git writes
 * them.
 */
inline constexpr std::uint32_t tree_mode = 0040000;
inline constexpr std::uint32_t symlink_mode = 0120000;
inline constexpr std::uint32_t gitlink_mode = 0160000;

/**
 * @brief One entry of a tree object. name views the bytes of the tree.
 */
struct tree_entry {
    std::uint32_t mode = 0;
    std::string_view name;
    object_id id;
};

/**
 * @brief Every path of a commit, as `git ls-tree -r` lists them: one row
 * per file, symbolic link and submodule, with its mode and ID, in the
 * byte order of the paths rather than git's tree order.
 */
struct commit_files {
    std::vector<std::string> paths;
    std::vector<std::uint32_t> modes;
    std::vector<object_id> ids;

    [[nodiscard]] std::size_t size() const noexcept { return paths.size(); }

    /**
     * @brief The row of @p path, or nothing when the commit has no such
     * path.
     */
    [[nodiscard]] std::optional<std::size_t> find(
        std::string_view path) const {
        const auto it = std::ranges::lower_bound(paths, path, {},
            [](const std::string& p) { return std::string_view(p); });
        if (it == paths.end() || *it != path)
            return std::nullopt;
        return static_cast<std::size_t>(it - paths.begin());
    }
};

}

#endif

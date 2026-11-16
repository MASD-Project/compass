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
#ifndef COMPASS_STORE_IO_GIT_OBJECTS_HPP
#define COMPASS_STORE_IO_GIT_OBJECTS_HPP

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.store/types/git_object.hpp"

namespace compass::store::io {

/**
 * @brief The object database of a git repository, read with no git
 * process: its pack files, through their indexes, and its loose objects,
 * with the repositories objects/info/alternates names, recursively.
 *
 * Every pack and index is mapped when the database is opened, so a pack
 * that git writes later is not seen, and a pack it removes stays readable
 * through the mapping. A delta is applied to its base, following chains of
 * offset and reference deltas to a whole object. Bases are kept in a
 * cache of bounded size shared by every thread, as git's delta base cache
 * is, so the objects of one chain do not inflate the chain again.
 * Objects are not hashed against their IDs when read: zlib's checksum and
 * the sizes the formats record catch damage.
 * read may be called from many threads at once.
 */
class git_objects final {
public:
    /**
     * @brief Opens the object database @p objects, the objects directory
     * of a repository: objects under its git common directory.
     * @throws std::system_error when a pack or index cannot be mapped, and
     * std::runtime_error when one is not in a format this reader reads.
     */
    explicit git_objects(const std::filesystem::path& objects);
    ~git_objects();

    git_objects(const git_objects&) = delete;
    git_objects& operator=(const git_objects&) = delete;

    /**
     * @brief The object @p id, or nothing when the database does not hold
     * it.
     * @throws std::runtime_error naming the object when it is held and
     * cannot be read: a damaged stream, a delta that does not apply, a
     * reference delta whose base is missing.
     */
    [[nodiscard]] std::optional<types::git_object> read(
        const types::object_id& id) const;

    /**
     * @brief The number of pack files opened.
     */
    [[nodiscard]] std::size_t packs() const noexcept;

private:
    struct state;
    std::unique_ptr<state> state_;
};

/**
 * @brief The objects directory of the repository whose git common
 * directory is @p git_common_dir.
 */
[[nodiscard]] std::filesystem::path git_objects_dir(
    const std::filesystem::path& git_common_dir);

/**
 * @brief Every path of the commit @p commit, as types::commit_files lists
 * them, reading its tree and every tree under it.
 * @throws std::runtime_error when the commit or a tree is missing, is of
 * another type or does not parse.
 */
[[nodiscard]] types::commit_files list_commit(const git_objects& db,
    const types::object_id& commit);

/**
 * @brief The bytes of the file at @p path of the commit @p commit, reading
 * only the trees on the way to it, or nothing when the commit has no file
 * there. A symbolic link's bytes are its target.
 * @throws as list_commit does, and when the file's blob is missing.
 */
[[nodiscard]] std::optional<std::string> read_commit_file(
    const git_objects& db, const types::object_id& commit,
    std::string_view path);

}

#endif

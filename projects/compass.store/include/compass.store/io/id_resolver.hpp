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
#ifndef COMPASS_STORE_IO_ID_RESOLVER_HPP
#define COMPASS_STORE_IO_ID_RESOLVER_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.store/types/blob_id.hpp"
#include "compass.store/types/git_index.hpp"
#include "compass.store/types/git_settings.hpp"
#include "compass.store/types/worktree_index.hpp"

namespace compass::store::io {

/**
 * @brief Where the ID of a file came from: git's index, the worktree index
 * compass keeps for the files it hashed, or the file's bytes.
 */
enum class id_source : std::uint8_t { git_index, worktree_index, hashed };

/**
 * @brief The ID of a file, where it came from, and the stat data the file
 * had when it was resolved, which is missing when the file is not a
 * regular file.
 */
struct resolved_id {
    types::blob_id id;
    id_source source = id_source::hashed;
    std::optional<types::stat_data> stat;
};

/**
 * @brief Gives the blob ID of each file of a checkout: from git's index
 * when git would call the file clean and its index ID names its bytes,
 * else from the worktree index when the file is unchanged since compass
 * last hashed it, else by hashing the file.
 *
 * The index ID is used only for a stage-0 entry of a regular file, not
 * added with intent to add, whose stat data matches the file and which is
 * not racily clean (see ops::is_clean). It is never used when the
 * repository's configuration sets core.autocrlf to true or input or uses an
 * object format other than SHA-1, nor for a path an attributes rule may
 * convert (see ops::read_attributes). A path with conflicts, an untracked
 * path and a missing index all mean the worktree index or hashing.
 *
 * The worktree index lives at compass/index under the checkout's own git
 * directory, so each worktree has its own. An entry is used when every stat
 * field matches the file and the file was modified before the second the
 * run that wrote the entry started (see ops::is_unchanged). A file that is
 * missing, cut short, of another version or whose checksum does not match
 * is treated as empty.
 *
 * Construction reads the index of the checkout's own git directory, the
 * worktree index beside it, the system, global and repository configuration
 * with their includes, and the attributes files: the system and global
 * ones, info/attributes, and the .gitattributes of every directory that
 * holds a tracked path. It also notes the time, which becomes the written
 * time of the next worktree index. Lookups do not change the resolver and
 * may run on many threads.
 */
class id_resolver final {
public:
    /**
     * @throws std::system_error when the git index exists but cannot be
     * read. A worktree index that cannot be read is treated as empty.
     */
    explicit id_resolver(const std::filesystem::path& checkout);

    /**
     * @brief The ID of @p path, relative to the checkout, from git's index
     * or the worktree index, or nothing when the file must be hashed.
     * Costs one lstat, or none for a path neither index holds.
     */
    [[nodiscard]] std::optional<resolved_id> known_id(
        std::string_view path) const;

    /**
     * @brief The ID of @p path, from known_id when it gives one, else by
     * hashing the file.
     *
     * @throws std::system_error when the file must be hashed and cannot be
     * read.
     */
    [[nodiscard]] resolved_id resolve(std::string_view path) const;

    /**
     * @brief Replaces the worktree index with the entries of this run and
     * returns how many it holds.
     *
     * An entry is kept for each of @p paths whose ID in @p resolved did not
     * come from git's index and which was a regular file. An entry of the
     * previous worktree index for a path not in @p paths is carried over
     * when the file is still unchanged, and dropped otherwise, so the
     * entries of removed files go. The file is written to a temporary file
     * and renamed into place: when two runs in one worktree write at once,
     * the last rename wins and the other's entries are hashed again next
     * time.
     *
     * @pre @p paths and @p resolved have one row per file, with no path
     * twice.
     * @throws std::filesystem::filesystem_error when the file cannot be
     * written.
     */
    std::size_t write_worktree_index(std::span<const std::string_view> paths,
        std::span<const resolved_id> resolved) const;

    [[nodiscard]] const types::git_index& index() const noexcept {
        return index_;
    }

    [[nodiscard]] const types::worktree_index& worktree_index()
        const noexcept {
        return worktree_;
    }

    [[nodiscard]] const types::git_config& config() const noexcept {
        return config_;
    }

    /**
     * @brief Why the index could not be used, or empty when it was read.
     * When set, no file takes its ID from git's index.
     */
    [[nodiscard]] const std::string& index_problem() const noexcept {
        return index_problem_;
    }

    /**
     * @brief Why the worktree index could not be used, or empty when it was
     * read or did not exist. When set, it is treated as empty.
     */
    [[nodiscard]] const std::string& worktree_index_problem() const noexcept {
        return worktree_problem_;
    }

    /**
     * @brief How many index entries may lend their ID: those that pass
     * every rule that does not depend on the file's stat data.
     */
    [[nodiscard]] std::size_t trusted_entries() const noexcept {
        return trusted_.size();
    }

private:
    std::filesystem::path checkout_;
    std::filesystem::path worktree_file_;
    types::index_time started_;
    types::git_config config_;
    types::git_index index_;
    std::string index_problem_;
    types::attribute_rules attributes_;
    std::vector<std::uint32_t> trusted_;
    types::worktree_index worktree_;
    std::string worktree_problem_;
};

}

#endif

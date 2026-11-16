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
#include <string>
#include <string_view>
#include <vector>
#include "compass.store/types/blob_id.hpp"
#include "compass.store/types/git_index.hpp"
#include "compass.store/types/git_settings.hpp"

namespace compass::store::io {

/**
 * @brief Where the ID of a file came from.
 */
enum class id_source : std::uint8_t { index, hashed };

struct resolved_id {
    types::blob_id id;
    id_source source = id_source::hashed;
};

/**
 * @brief Gives the blob ID of each file of a checkout: from git's index
 * when git would call the file clean and its index ID names its bytes,
 * otherwise by hashing the file.
 *
 * The index ID is used only for a stage-0 entry of a regular file, not
 * added with intent to add, whose stat data matches the file and which is
 * not racily clean (see ops::is_clean). It is never used when the
 * repository's configuration sets core.autocrlf to true or input or uses an
 * object format other than SHA-1, nor for a path an attributes rule may
 * convert (see ops::read_attributes). A path with conflicts, an untracked
 * path and a missing index all mean hashing.
 *
 * Construction reads the index of the checkout's own git directory, the
 * system, global and repository configuration with their includes, and the
 * attributes files: the system and global ones, info/attributes, and the
 * .gitattributes of every directory that holds a tracked path. Lookups do
 * not change the resolver and may run on many threads.
 */
class id_resolver final {
public:
    /**
     * @throws std::runtime_error when the index exists but is not a
     * well-formed index of a supported version, and std::system_error when
     * it cannot be read.
     */
    explicit id_resolver(const std::filesystem::path& checkout);

    /**
     * @brief The ID of @p path, relative to the checkout, from the index,
     * or nothing when the file must be hashed. Costs one lstat.
     */
    [[nodiscard]] std::optional<types::blob_id> index_id(
        std::string_view path) const;

    /**
     * @brief The ID of @p path, from the index when index_id gives one,
     * else by hashing the file.
     *
     * @throws std::system_error when the file must be hashed and cannot be
     * read.
     */
    [[nodiscard]] resolved_id resolve(std::string_view path) const;

    [[nodiscard]] const types::git_index& index() const noexcept {
        return index_;
    }

    [[nodiscard]] const types::git_config& config() const noexcept {
        return config_;
    }

    /**
     * @brief Why the index could not be used, or empty when it was read.
     * When set, every file is hashed.
     */
    [[nodiscard]] const std::string& index_problem() const noexcept {
        return index_problem_;
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
    types::git_config config_;
    types::git_index index_;
    std::string index_problem_;
    types::attribute_rules attributes_;
    std::vector<std::uint32_t> trusted_;
};

}

#endif

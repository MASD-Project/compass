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
#ifndef COMPASS_STORE_IO_PACK_STORE_HPP
#define COMPASS_STORE_IO_PACK_STORE_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.platform/filesystem/file_lock.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.platform/filesystem/new_file.hpp"
#include "compass.store/ops/pack.hpp"
#include "compass.store/types/pack.hpp"

namespace compass::store::io {

/**
 * @brief Whether @p name is the name of a published pack: "pack-", 32 hex
 * digits and ".cpk". Anything else in the packs directory is not read.
 */
[[nodiscard]] bool is_pack_name(std::string_view name) noexcept;

/**
 * @brief One published pack, mapped and checked.
 */
struct open_pack {
    std::filesystem::path path;
    platform::filesystem::mapped_file file;
    ops::pack_view view;
};

/**
 * @brief Every pack of @p packs_dir that parses, mapped, in name order; a
 * file that does not parse is skipped, and named in @p rejected when
 * given.
 *
 * The directory is listed until two listings in a row agree and no pack
 * listed vanished before it was opened, each name being opened once. A
 * merge publishes its pack before it removes those it merged, so a reader
 * racing it holds the merged packs or the merge's pack, and never loses
 * an entry to it. After a thousand listings under constant change the
 * reader keeps what it holds; an entry it then lacks is a cache miss.
 */
[[nodiscard]] std::vector<open_pack> open_packs(
    const std::filesystem::path& packs_dir,
    std::vector<std::filesystem::path>* rejected = nullptr);

/**
 * @brief The entries of every pack of a store, as they were when it was
 * opened.
 *
 * A pack published after opening is not seen; one removed after opening
 * stays readable, since its mapping outlives the unlink. Lookups only read
 * the mappings, so any number of threads may share a reader.
 */
class pack_reader final {
public:
    explicit pack_reader(const std::filesystem::path& packs_dir);

    /**
     * @brief The bytes of the entry of @p kind and @p key, valid while the
     * reader lives; of a manifest, the one written last.
     */
    [[nodiscard]] std::optional<std::span<const std::byte>> find(
        types::entry_kind kind, const types::blob_id& key) const noexcept;

    /**
     * @brief The body of the action object of @p kind under @p key, or
     * nothing when it is missing or does not decode.
     */
    [[nodiscard]] std::optional<std::string> read_action(
        types::action_kind kind, const types::blob_id& key) const;

    [[nodiscard]] std::size_t pack_count() const noexcept {
        return packs_.size();
    }

private:
    std::vector<open_pack> packs_;
};

/**
 * @brief Builds one pack and publishes it whole, or not at all.
 *
 * Entries are appended to a temporary file named tmp_<random> in the packs
 * directory; publish writes the index and renames the file to
 * pack-<time><random>.cpk, a name no other writer takes, so writers in any
 * number of processes and worktrees never share a file and need no lock.
 * add may be called from many threads. A writer destroyed unpublished
 * removes its temporary file; one a crash cut short leaves it for
 * collection.
 */
class pack_writer final {
public:
    explicit pack_writer(std::filesystem::path packs_dir);
    ~pack_writer();

    pack_writer(const pack_writer&) = delete;
    pack_writer& operator=(const pack_writer&) = delete;

    /**
     * @brief Appends @p bytes as the entry of @p kind and @p key, written
     * at @p written, or now when it is zero.
     */
    void add(types::entry_kind kind, const types::blob_id& key,
        std::span<const std::byte> bytes, std::int64_t written = 0);

    /**
     * @brief Appends @p body framed as an action object.
     */
    void add_action(types::action_kind kind, const types::blob_id& key,
        std::string_view body);

    [[nodiscard]] std::size_t size() const;

    /**
     * @brief Writes the index and publishes the pack, then repacks the
     * store; nothing when no entry was added.
     * @return the path of the pack.
     * @throws std::system_error when a write or the rename fails.
     */
    std::optional<std::filesystem::path> publish();

    /**
     * @brief As publish, without the repack that follows.
     */
    std::optional<std::filesystem::path> publish_only();

private:
    std::filesystem::path dir_;
    std::filesystem::path temporary_;
    std::optional<platform::filesystem::new_file> file_;
    std::uint64_t offset_ = 0;
    std::vector<types::pack_entry> entries_;
    mutable std::mutex mutex_;
};

/**
 * @brief An exclusive hold on the lock file of a packs directory, which
 * the system releases when its holder dies. Repacking and collection take
 * it; writing and reading never do.
 */
class pack_lock final {
public:
    /**
     * @brief Takes the lock of @p packs_dir, waiting for it when @p wait,
     * else giving up at once when another process holds it.
     */
    pack_lock(const std::filesystem::path& packs_dir, bool wait);

    [[nodiscard]] bool held() const noexcept { return lock_.held(); }

private:
    platform::filesystem::file_lock lock_;
};

/**
 * @brief Writes the entries @p keep of @p packs into one new pack of
 * @p packs_dir, then removes @p packs. Of entries of one kind and key the
 * one written last is kept. The caller holds the lock.
 * @return the new pack, or nothing when no entry was kept.
 */
std::optional<std::filesystem::path> rewrite_packs(
    const std::filesystem::path& packs_dir,
    std::span<const open_pack> packs,
    std::span<const std::vector<std::uint8_t>> keep);

/**
 * @brief Merges the smallest packs of @p packs_dir as
 * ops::packs_to_merge says, when no other process holds its lock.
 * @return the number of packs merged.
 */
std::size_t repack(const std::filesystem::path& packs_dir);

}

#endif

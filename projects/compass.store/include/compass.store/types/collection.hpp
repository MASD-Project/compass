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
#ifndef COMPASS_STORE_TYPES_COLLECTION_HPP
#define COMPASS_STORE_TYPES_COLLECTION_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "compass.store/types/blob_id.hpp"

namespace compass::store::types {

/**
 * @brief What a store holds, as parallel columns, one row each: the entries
 * of its packs and the files beside them that are not packs.
 *
 * name is "object/<hex>", "manifest/<hex>" or "result/<hex>" for an entry,
 * and the file name for anything else: a temporary file a crashed write
 * left behind or a pack that does not parse, which has temporary set to 1
 * and a zero ID. mtime is when an entry was first written, or a file's
 * modification time, in nanoseconds since the epoch.
 */
struct stored_objects {
    std::vector<std::string> names;
    std::vector<blob_id> ids;
    std::vector<std::uint8_t> temporary;
    std::vector<std::uint64_t> sizes;
    std::vector<std::int64_t> mtimes;

    [[nodiscard]] std::size_t size() const noexcept { return names.size(); }
};

/**
 * @brief A number of files and the bytes they hold.
 */
struct object_tally {
    std::uint64_t objects = 0;
    std::uint64_t bytes = 0;

    void add(std::uint64_t size) noexcept {
        ++objects;
        bytes += size;
    }
};

/**
 * @brief Which files of a store a collection removes, and why the others
 * stay.
 *
 * Every object is live, or unreferenced and within the grace period, or
 * unreferenced and old. victims are rows of the listing, oldest first: every
 * temporary file older than the grace period, and old unreferenced objects
 * until the store is at or under the cap.
 */
struct collection_plan {
    object_tally stored;
    object_tally live;
    object_tally within_grace;
    object_tally old_unreferenced;
    object_tally temporaries;
    object_tally victims;
    std::vector<std::uint32_t> victim_rows;
};

/**
 * @brief How a collection runs. cap is the size in bytes the store must
 * come down to, so zero removes every old unreferenced object. A file
 * modified less than grace ago is never removed. A dry run removes nothing.
 * wait says whether to wait for another process that holds the lock of
 * the packs, or else to give up at once.
 */
struct collection_options {
    std::uint64_t cap = 0;
    std::chrono::seconds grace{3600};
    bool dry_run = false;
    bool wait = true;
};

/**
 * @brief What a collection found and did, with the wall time of each phase.
 *
 * worktrees are the git directories whose indexes were read. live_ids counts
 * the distinct IDs they refer to, and live_keys the distinct action keys
 * their records name. actions and live_actions count the action objects
 * stored and those a record names; the plan counts them with the objects.
 * superseded counts the entries a later one of the same kind and key
 * hides, which nothing reads and every collection drops. victims names
 * what the plan chose, oldest first, and removed counts the entries and
 * files actually dropped: none in a dry run. packs and packs_after count
 * the packs before and after; legacy counts the files of the first layout
 * of the store, which a collection removes whole. skipped is true when
 * another process held the lock and the collection did not wait for it,
 * so it removed nothing.
 */
struct collection_result {
    std::vector<std::filesystem::path> worktrees;
    std::uint64_t live_ids = 0;
    std::uint64_t live_keys = 0;
    object_tally actions;
    object_tally live_actions;
    collection_plan plan;
    object_tally superseded;
    std::vector<std::string> victims;
    object_tally removed;
    std::uint64_t packs = 0;
    std::uint64_t packs_after = 0;
    object_tally legacy;
    bool skipped = false;
    double worktrees_ms = 0;
    double live_ms = 0;
    double list_ms = 0;
    double plan_ms = 0;
    double remove_ms = 0;
};

}

#endif

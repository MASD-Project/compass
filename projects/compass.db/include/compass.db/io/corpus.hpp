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
#ifndef COMPASS_DB_IO_CORPUS_HPP
#define COMPASS_DB_IO_CORPUS_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "compass.db/io/connection.hpp"
#include "compass.db/types/corpus_document.hpp"
#include "compass.db/types/environment.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::db::io {

/**
 * @brief The IDs among @p wanted whose document the corpus holds, in no
 * particular order.
 */
[[nodiscard]] std::vector<store::types::blob_id> present(connection& c,
    std::span<const store::types::blob_id> wanted);

/**
 * @brief What writing an environment did. When retry is set nothing was
 * written: missing holds the IDs the placements name that are neither in
 * the corpus nor among the documents given, because a collection removed
 * them after present() was asked; the caller loads them and writes again.
 */
struct write_result {
    bool retry = false;
    std::vector<store::types::blob_id> missing;
    std::uint64_t inserted = 0;
    std::uint64_t added = 0;
    std::uint64_t changed = 0;
    std::uint64_t removed = 0;
};

/**
 * @brief Writes @p documents into the corpus and makes @p placements what
 * the environment @p e holds, in one transaction, creating the
 * environment when it is new.
 *
 * When @p scope is empty the placements replace every path of the
 * environment; otherwise only the paths of @p scope change, each to its
 * placement or, when it has none, out of the environment. Documents
 * already present are left as they are (ON CONFLICT DO NOTHING), so two
 * syncs may write one document at once. Every document a placement names
 * is locked against collection until the transaction ends. Readers see
 * the old placements or the new ones, never a mix. head and synced_at are
 * set; head only when @p e has one.
 *
 * @throws error when a statement fails; nothing is written then.
 */
write_result write_environment(connection& c, const types::environment& e,
    std::span<const types::corpus_document> documents,
    std::span<const types::placement> placements,
    std::span<const std::string> scope = {});

/**
 * @brief What a collection did. skipped is set when another collection
 * held the lock, so this one waited for nothing and removed nothing.
 */
struct collection_result {
    bool ran = false;
    bool skipped = false;
    std::uint64_t removed = 0;
    std::uint64_t left = 0;
};

/**
 * @brief The number of documents below which collection as part of a sync
 * never runs.
 */
inline constexpr std::uint64_t collection_floor = 4096;

/**
 * @brief Removes every document no placement and no activity.run names,
 * with its headings, sections, links and test definition, unless another
 * collection holds the lock, and records how many documents are left. A
 * document a sync has locked is kept.
 */
collection_result collect(connection& c);

/**
 * @brief Collects as collect() does when the corpus holds at least
 * collection_floor documents and at least twice what the last collection
 * left, or none was recorded; otherwise does nothing. So the corpus is
 * swept at most once per doubling.
 */
collection_result collect_when_grown(connection& c);

/**
 * @brief Every environment, by name, with how many paths each places.
 */
[[nodiscard]] std::vector<types::environment_summary> environments(
    connection& c);

/**
 * @brief Drops the environment @p name and its placements; its documents
 * stay until a collection. Returns false when there is no such
 * environment.
 */
bool drop_environment(connection& c, const std::string& name);

}

#endif

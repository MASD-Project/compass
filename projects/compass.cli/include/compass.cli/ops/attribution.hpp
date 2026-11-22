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
#ifndef COMPASS_CLI_OPS_ATTRIBUTION_HPP
#define COMPASS_CLI_OPS_ATTRIBUTION_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include "compass.cli/types/results.hpp"
#include "compass.codec/types/dependency_record.hpp"
#include "compass.codec/types/output_set.hpp"
#include "compass.store/types/blob_id.hpp"

/**
 * What produced each file generate writes, whether the change is the
 * checkout's own or drift HEAD already had, and which outputs a scope
 * takes. Pure functions over the dependency record and the outputs of a
 * run; app::generate and app::diff run git and the passes.
 */
namespace compass::cli::ops {

/**
 * @brief The unit of a dependency record that produced a file, and the
 * row of that file among the record's files.
 */
struct producer {
    std::uint32_t unit = 0;
    codec::types::file_id file;
};

/**
 * @brief The unit of @p record that produced each of its files, by the
 * file's path relative to the output root. Of two units that produce one
 * path, the later, as codec::ops::outputs_of takes it.
 */
[[nodiscard]] std::unordered_map<std::string_view, producer>
producers_of(const codec::types::dependency_record& record);

/**
 * @brief Whether the path @p path matches the listing pattern
 * @p pattern: a prefix and a suffix joined by '*', or a prefix alone.
 */
[[nodiscard]] bool matches_listing(std::string_view pattern,
    std::string_view path) noexcept;

/**
 * @brief Flags, one per unit of @p record, set for each unit that read one
 * of @p paths, files of the checkout: a unit of that document, or one that
 * looked up a document, a file or the existence of a file at it, or listed
 * a pattern it matches. A read of every document of a type, an ID lookup
 * or a read of the type registry does not count, as those read every
 * document rather than one.
 */
[[nodiscard]] std::vector<std::uint8_t> units_reading(
    const codec::types::dependency_record& record,
    std::span<const std::string> paths);

/**
 * @brief Why a file changes, from three blob IDs: @p generated, what the
 * checkout generates at its path, none when it generates nothing there;
 * @p at_head, what HEAD generates there; and @p committed, what HEAD
 * holds there. A file the checkout generates differently from HEAD is
 * yours. Otherwise it is stale when HEAD's file differs from what HEAD
 * generates, or neither generates it, and yours when HEAD's file was
 * current, so the file was edited since.
 */
[[nodiscard]] types::change_cause cause_of(
    const std::optional<store::types::blob_id>& generated,
    const std::optional<store::types::blob_id>& at_head,
    const std::optional<store::types::blob_id>& committed);

/**
 * @brief Drops from @p outputs each row whose index @p keep refuses, and
 * takes the codec of each dropped row out of complete, so writing what is
 * left leaves the files of that codec that it does not hold as they are,
 * rather than removing them.
 */
void narrow(codec::types::output_set& outputs,
    const std::function<bool(std::size_t row)>& keep);

/**
 * @brief The label of what produced @p f: its codec, then its unit and
 * its origin when it has them, as "codegen, a.org, t.mustache".
 */
[[nodiscard]] std::string producer_label(const types::attributed_file& f);

/**
 * @brief The files of @p files whose cause is @p cause, grouped by what
 * produced them: the codec and the origin, else the codec and the unit,
 * or the codec alone when that would make more than @p most groups. Each
 * group is its label and the indexes of its files, in their order; the
 * groups come largest first, then by label.
 */
[[nodiscard]] std::vector<std::pair<std::string, std::vector<std::size_t>>>
groups_of(std::span<const types::attributed_file> files,
    types::change_cause cause, std::size_t most);

}

#endif

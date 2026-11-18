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
#ifndef COMPASS_CODEC_IO_FORMAT_CACHE_HPP
#define COMPASS_CODEC_IO_FORMAT_CACHE_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "compass.codec/types/file_table.hpp"
#include "compass.codec/types/formatter.hpp"
#include "compass.codec/types/registry.hpp"
#include "compass.codec/types/run_result.hpp"

namespace compass::codec::io {

/**
 * @brief What formatting a file table did.
 *
 * hits counts files whose formatted form the store held, formatted those
 * the formatters ran over and unformatted those no formatter applies to.
 * runs counts the formatter processes started, version probes included:
 * zero when every file was a hit and every program's version was known.
 */
struct format_stats {
    std::uint64_t hits = 0;
    std::uint64_t formatted = 0;
    std::uint64_t unformatted = 0;
    std::uint64_t runs = 0;
    double format_ms = 0;
};

/**
 * @brief The files of a table formatted, row for row, the blob ID of each
 * and what it took.
 */
struct formatted_files {
    types::file_table files;
    std::vector<compass::store::types::blob_id> ids;
    format_stats stats;
};

/**
 * @brief The largest number of files one formatter process is given.
 */
inline constexpr std::size_t format_batch = 512;

/**
 * @brief Formats each file of @p raw with the formatter of @p formatters
 * its extension selects, through the result cache of the repository whose
 * git common directory is @p git_common_dir.
 *
 * A formatted form is an action result keyed by ops::formatted_key over the
 * formatter's key, the file's path and the blob ID of the raw bytes. A
 * program's version is read with --version once and kept as an action
 * result keyed by ops::program_key, so a run that finds every key runs no
 * program at all.
 * The misses of each formatter are written under a fresh temporary
 * directory at their own paths, so a formatter that reads the file name
 * sees the real one, and formatted there in batches of at most
 * format_batch files, every pass of a batch in one process each, @p threads
 * batches at a time. The formatter runs in that directory. Rows of one key
 * are formatted once; rows that share a path but not a key are each
 * formatted from their own bytes.
 *
 * @throws std::runtime_error naming the formatter when it exits with an
 * error, and std::system_error when it cannot be started.
 */
[[nodiscard]] formatted_files format_files(
    const std::filesystem::path& git_common_dir,
    std::span<const types::formatter> formatters,
    const types::file_table& raw, unsigned threads);

/**
 * @brief Formats, in place, the files @p run holds for each enabled codec
 * of @p r whose descriptor has formatters, with the formatters its option
 * values give for the checkout @p checkout, through format_files and the
 * result cache of @p git_common_dir, and sets the codec's formatted_ids.
 * The record of @p run keeps the bytes the codecs emitted.
 * @return per codec, what formatting its files did, or nothing for a codec
 * that was not formatted.
 * @throws as format_files does, and what a formatters function throws.
 */
[[nodiscard]] std::vector<std::optional<format_stats>> format_outputs(
    const types::registry& r, types::run_result& run,
    const std::filesystem::path& checkout,
    const std::filesystem::path& git_common_dir, unsigned threads);

}

#endif

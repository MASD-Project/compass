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
#ifndef COMPASS_CODEC_IO_OUTPUT_SINK_HPP
#define COMPASS_CODEC_IO_OUTPUT_SINK_HPP

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "compass.codec/types/output_set.hpp"
#include "compass.codec/types/written_outputs.hpp"

namespace compass::codec::io {

/**
 * @brief The record of written outputs of one checkout: compass/outputs
 * under its own @p git_dir, beside the dependency record.
 */
[[nodiscard]] std::filesystem::path written_outputs_path(
    const std::filesystem::path& git_dir);

/**
 * @brief What reading the record of written outputs found.
 * problem is empty when the file was read or is missing; otherwise it says
 * why the file was not used, and record is empty. bytes is the size of the
 * file.
 */
struct loaded_outputs {
    types::written_outputs record;
    std::string problem;
    std::uint64_t bytes = 0;
};

/**
 * @brief Reads the record at @p path. A missing file is an empty record; a
 * damaged one, or one of another version, is an empty record with a
 * problem. Never throws for the content of the file.
 */
[[nodiscard]] loaded_outputs read_written_outputs(
    const std::filesystem::path& path);

/**
 * @brief Puts @p record at @p path unless the file already holds exactly
 * these bytes, through a temporary file and a rename.
 * @return true when the file was written.
 * @throws std::filesystem::filesystem_error when the directory, the file or
 * the rename fails.
 */
bool write_written_outputs(const std::filesystem::path& path,
    const types::written_outputs& record);

/**
 * @brief What putting a run's outputs on disk did.
 *
 * problems is not empty when the outputs could not be written at all; then
 * no file was touched and record is the previous one. Otherwise each
 * output was counted once: written when its bytes are new for its path,
 * restored when compass had written these bytes before and the file had
 * since been edited or deleted, unchanged when the file already held them
 * and was left alone. removed counts files compass wrote that no complete
 * codec produces any more; kept counts such files that were edited after
 * compass wrote them, which are left in place and forgotten. record is
 * the record for the next run.
 */
struct sink_result {
    std::vector<std::string> problems;
    std::uint64_t written = 0;
    std::uint64_t restored = 0;
    std::uint64_t unchanged = 0;
    std::uint64_t removed = 0;
    std::uint64_t kept = 0;
    types::written_outputs record;
};

/**
 * @brief Makes the files under @p root hold @p outputs, touching only the
 * files that differ.
 *
 * Nothing is written when ops::check_outputs finds a problem. Each output
 * whose recorded entry has the same blob ID and whose file has the recorded
 * stat data, and is not racily clean, is left alone without being read.
 * Any other output whose file exists is hashed, and left alone when it
 * already holds the bytes. Only then is it written, through a temporary
 * file in its directory and a rename, so its modification time changes
 * only when its content does.
 *
 * An entry of @p previous whose codec is complete in @p outputs and whose
 * path no output names is removed when its file still holds what compass
 * wrote, and its emptied directories with it, up to @p root. Entries of
 * other codecs are carried over. @p previous is ignored when it was
 * recorded for another root, and then no file is removed.
 *
 * The record's written time is the start of this call when an entry
 * changed or an entry was racily clean, and the previous one otherwise,
 * so a run that changes nothing yields the same record.
 *
 * @p root must be absolute. A final path component that is a symbolic
 * link is replaced by the rename, never written through.
 * @throws std::filesystem::filesystem_error or std::system_error when a
 * file cannot be read, written or removed.
 */
[[nodiscard]] sink_result write_outputs(const std::filesystem::path& root,
    const types::output_set& outputs,
    const types::written_outputs& previous);

}

#endif

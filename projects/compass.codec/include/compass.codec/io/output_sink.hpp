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

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/ops/hand_edits.hpp"
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
 * @brief What putting a run's outputs on disk does to one file.
 *
 * write puts bytes that are new for the path; restore puts back bytes
 * compass wrote before, after the file was edited or deleted; unchanged
 * leaves a file that already holds its bytes. remove deletes a file
 * compass wrote that no complete codec produces any more; keep leaves such
 * a file because it was edited after compass wrote it, or because its path
 * passes through a symbolic link.
 */
enum class output_action : std::uint8_t {
    write, restore, unchanged, remove, keep
};

/**
 * @brief Whether @p a changes the file it names.
 */
[[nodiscard]] constexpr bool changes_file(output_action a) noexcept {
    return a == output_action::write || a == output_action::restore ||
        a == output_action::remove;
}

/**
 * @brief What putting a run's outputs on disk would do, decided without
 * changing anything.
 *
 * problems is not empty when the outputs cannot be written at all; then
 * there are no actions and record is the previous one. Otherwise there is
 * one action per output, in the order of the outputs, then one per file
 * compass wrote that no complete codec produces any more and that still
 * exists, in path order. contents holds the bytes of write, restore and
 * unchanged, and is empty for remove and keep.
 *
 * record is the record for the next run, except that the stat data of a
 * file still to be written or restored is the old one until apply_outputs
 * writes it.
 *
 * The views point into the outputs and the previous record the plan was
 * made from, which must outlive it.
 */
struct output_plan {
    std::vector<std::string> problems;
    std::vector<std::string_view> paths;
    std::vector<std::string_view> codecs;
    std::vector<output_action> actions;
    std::vector<std::string_view> contents;
    types::written_outputs record;

    [[nodiscard]] std::size_t size() const noexcept { return paths.size(); }
};

/**
 * @brief What putting a run's outputs on disk did, or would do.
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
 * @brief Decides what makes the files under @p root hold @p outputs,
 * reading the files but changing none.
 *
 * Nothing is planned when ops::check_outputs finds a problem. Each output
 * whose recorded entry has the same blob ID and whose file has the recorded
 * stat data, and is not racily clean, is unchanged without being read.
 * Any other output whose file exists is hashed, and is unchanged when it
 * already holds the bytes.
 *
 * An entry of @p previous whose codec is complete in @p outputs and whose
 * path no output names is removed when its file still holds what compass
 * wrote, kept otherwise, and dropped from the record when its file is
 * gone. Entries of other codecs are carried over. @p previous is ignored
 * when it was recorded for another root, and then nothing is removed.
 *
 * The record's written time is the start of this call when an entry
 * changes or an entry was racily clean, and the previous one otherwise,
 * so a run that changes nothing yields the same record.
 *
 * @p root must be absolute.
 * @throws std::filesystem::filesystem_error or std::system_error when a
 * file cannot be read.
 */
[[nodiscard]] output_plan plan_outputs(const std::filesystem::path& root,
    const types::output_set& outputs,
    const types::written_outputs& previous);

/**
 * @brief The counts and the record of @p plan, as applying it reports
 * them, except for the stat data of the files it would write.
 */
[[nodiscard]] sink_result summarise(const output_plan& plan);

/**
 * @brief Performs @p plan: writes and restores through
 * platform::filesystem::update_file, and removes files with their emptied
 * directories, up to the root. A file whose content is unchanged is never
 * opened for writing, even when the plan says to write it, so it keeps
 * its modification time, inode and mode; a file that is written keeps its
 * mode. A run killed partway leaves each file whole, old or new, and
 * leaves untouched every file it had not reached. A final path component
 * that is a symbolic link is replaced by the rename, never written
 * through.
 * @throws std::filesystem::filesystem_error or std::system_error when a
 * file cannot be written or removed.
 */
[[nodiscard]] sink_result apply_outputs(const output_plan& plan);

/**
 * @brief Makes the files under @p root hold @p outputs, touching only the
 * files that differ: apply_outputs of plan_outputs.
 */
[[nodiscard]] sink_result write_outputs(const std::filesystem::path& root,
    const types::output_set& outputs,
    const types::written_outputs& previous);

/**
 * @brief The unified diff, in git's format, of every action of @p plan
 * that changes a file, against the file on disk, in the order of the plan.
 * @throws std::filesystem::filesystem_error when a file exists but cannot
 * be read.
 */
[[nodiscard]] std::string diff_outputs(const output_plan& plan);

/**
 * @brief A file a plan rewrites and the block of it the rewrite deletes.
 */
struct hand_edit {
    std::string path;
    ops::dropped_block block;
};

/**
 * @brief Each file @p plan writes or restores that may hold code written
 * by hand which the write deletes, as ops::dropped_hand_lines judges it.
 * Only a file that is on disk and is not what compass last wrote there,
 * by @p previous, the record the plan was made from, is read: one compass
 * wrote and nobody touched holds nothing a person wrote.
 */
[[nodiscard]] std::vector<hand_edit> find_hand_edits(const output_plan& plan,
    const types::written_outputs& previous);

}

#endif

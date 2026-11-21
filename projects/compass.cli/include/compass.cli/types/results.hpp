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
#ifndef COMPASS_CLI_TYPES_RESULTS_HPP
#define COMPASS_CLI_TYPES_RESULTS_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "compass.codec/io/format_cache.hpp"
#include "compass.codec/io/output_sink.hpp"
#include "compass.codec/types/descriptor.hpp"
#include "compass.codec/types/diagnostic_table.hpp"
#include "compass.store/types/blob_id.hpp"
#include "compass.store/types/collection.hpp"
#include "compass.store/types/git_object.hpp"

namespace compass::cli::types {

/**
 * @brief What loading the corpus and running the codecs did.
 *
 * files counts the org files of the checkout, documents those that
 * loaded, loaded those whose object came from the store and parsed those
 * parsed instead. errors holds one line per file that could not be read
 * or parsed, and problems one line per defect of the codec registry, such
 * as a --set nobody declared; either makes the command fail. codecs
 * names each codec of the run and ran counts the units of that codec that
 * were called rather than reused, skipped those the worktree's record
 * stood in for and cached those the result cache stood in for, and
 * codec_ms the time spent in the codec's calls, summed over threads. formats
 * says, per codec, what formatting its files did, for a codec whose files
 * were formatted. steps names each step of the command with its wall
 * time, in the order they ran. collection is what the collection the
 * run started did, when the store had grown enough for one, and
 * collection_problem why one failed, which fails nothing. ms is the wall
 * time from listing the checkout to the end of the codec run.
 */
struct run_summary {
    std::uint64_t files = 0;
    std::uint64_t documents = 0;
    std::uint64_t loaded = 0;
    std::uint64_t parsed = 0;
    std::vector<std::string> errors;
    std::vector<std::string> problems;
    std::vector<std::string> codecs;
    std::vector<std::uint64_t> ran;
    std::vector<std::uint64_t> skipped;
    std::vector<std::uint64_t> cached;
    std::vector<double> codec_ms;
    std::vector<std::optional<codec::io::format_stats>> formats;
    std::vector<std::pair<std::string, double>> steps;
    std::optional<store::types::collection_result> collection;
    std::string collection_problem;
    double ms = 0;

    [[nodiscard]] bool failed() const noexcept {
        return !errors.empty() || !problems.empty();
    }
};

/**
 * @brief What compass generate did: sink counts what writing the outputs
 * under root did, each file once over every pass, and its problems why
 * nothing could be written or the outputs did not settle. passes counts
 * the runs of the codecs: generate runs them again while a pass changes
 * a file one of them read. run sums what every pass ran. hand_edits are
 * the files whose rewrite deleted what may be code written by hand, which
 * generate warns of. ms is the wall time of the whole command.
 */
struct generate_result {
    run_summary run;
    std::filesystem::path root;
    codec::io::sink_result sink;
    std::vector<codec::io::hand_edit> hand_edits;
    std::uint32_t passes = 1;
    double ms = 0;
};

/**
 * @brief What compass diff found: planned counts what generate would do,
 * and diff is the unified diff of every file it would change, over as
 * many passes as generate would make, which passes counts.
 */
struct diff_result {
    run_summary run;
    std::filesystem::path root;
    codec::io::sink_result planned;
    std::string diff;
    std::uint32_t passes = 1;
    double ms = 0;
};

/**
 * @brief The two sides of a drift finding, as blob IDs: file is what the
 * path holds, none when it is missing, and generated what the codec
 * generates for it, none when it no longer generates the file.
 */
struct drift_sides {
    std::optional<store::types::blob_id> file;
    std::optional<store::types::blob_id> generated;

    friend bool operator==(const drift_sides&, const drift_sides&) = default;
};

/**
 * @brief One diagnostic, located: line and column are 1-based, the column
 * counted in bytes. rule is the codec and rule name as codec/rule. drift
 * is set on a codec/drift finding only. subject and subject_text are the
 * diagnostic's, which compass blame reads.
 */
struct finding {
    std::string path;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
    std::string rule;
    codec::types::severity level = codec::types::severity::error;
    std::string message;
    std::string fix;
    std::optional<drift_sides> drift;
    std::string subject;
    std::string subject_text;
};

/**
 * @brief Whether any of @p findings is an error, and so fails the command
 * that found it; warnings alone fail none.
 */
[[nodiscard]] inline bool fails(const std::vector<finding>& findings) {
    return std::ranges::any_of(findings, [](const finding& f) {
        return f.level == codec::types::severity::error; });
}

/**
 * @brief Which blame methods check runs on the findings already on main:
 * none, the cheap ones (line, added and drift), or those and bisect.
 */
enum class blame_mode : std::uint8_t { off, cheap, bisect };

/**
 * @brief One trailer a commit states, or fails to: its key, its value,
 * empty when the commit has no such trailer, and, for a trailer that
 * names a document, that document's #+title:, when the checkout has it.
 */
struct stated_trailer {
    std::string key;
    std::string value;
    std::string title;
};

/**
 * @brief A commit blame names as the likely cause of a finding.
 *
 * label tells two candidates of one finding apart, and is empty when
 * there is one. id is the commit in hex, empty for a change not yet
 * committed. pull_request is the number of the pull request that brought
 * it into main. trailers are those the commit_provenance check document
 * names, in its order, and environment the value of the one that names
 * who to tell, empty when not stated. on_main says main holds the
 * commit; one only the branch holds is the branch's own. inputs are the
 * inputs of a drift candidate that the commit changed.
 */
struct culprit {
    std::string label;
    std::string id;
    std::string subject;
    std::string author;
    std::optional<std::uint64_t> pull_request;
    std::vector<stated_trailer> trailers;
    std::string environment;
    bool on_main = false;
    std::vector<std::string> inputs;
};

/**
 * @brief One finding and what blame made of it: the method that found the
 * culprits, the subject an added blame followed, and the culprits, most
 * likely first; note says why there is none, or what the method fell
 * back from.
 */
struct blamed_finding {
    finding found;
    codec::types::blame_method method = codec::types::blame_method::bisect;
    std::string subject;
    std::vector<culprit> culprits;
    std::string note;
};

/**
 * @brief What blaming the findings of a run found. notify is the trailer
 * whose value names who to tell, empty when no check document names the
 * trailers. probes counts the checks bisect ran, and ms is the wall time
 * of the whole blame.
 */
struct blame_report {
    std::vector<blamed_finding> findings;
    std::string notify;
    std::size_t probes = 0;
    double ms = 0;
};

/**
 * @brief What compass check found, sorted by path, line and column, less
 * the excused findings a valid exception covers, and what blaming them
 * found when check blames.
 */
struct check_result {
    run_summary run;
    std::vector<finding> findings;
    std::size_t excused = 0;
    blame_report blame;
    double ms = 0;
};

/**
 * @brief What compass hook found for one edited file.
 * path is the file, relative to the checkout. checked is false when no
 * codec reads or reports on it, so nothing ran. findings are those on the
 * file and those the edit caused on other files, less the excused ones,
 * sorted as check sorts them. old counts the findings on the file that
 * the last check recorded, which are left out unless all were asked for.
 * note, when not empty, says why the hook could not check the file; it
 * then has no findings.
 */
struct hook_result {
    run_summary run;
    std::string path;
    bool checked = false;
    std::vector<finding> findings;
    std::size_t excused = 0;
    std::size_t old = 0;
    std::string note;
    double ms = 0;
};

/**
 * @brief A file the merge base holds at from and HEAD at to.
 */
struct rename {
    std::string from;
    std::string to;

    friend bool operator==(const rename&, const rename&) = default;
};

/**
 * @brief The findings of a check split against those of an earlier
 * check: fresh are the ones it did not have, in their order, and excused
 * counts the others, which inherited holds.
 */
struct ratchet_split {
    std::vector<finding> fresh;
    std::size_t excused = 0;
    std::vector<finding> inherited;
};

/**
 * @brief What checking the provenance of a branch found.
 *
 * checked is false when the check did not run, and note then says why.
 * commits counts the commits checked and merges those skipped as merges.
 * pull_request is the number of the pull request whose table was read;
 * when there is none, note says why. findings are ordinary findings,
 * their path commit/<id> for a commit, by the first twelve digits of its
 * ID, pull/<number> for the pull request, and branch/<name> for a branch
 * that has none.
 */
struct provenance_result {
    bool checked = false;
    std::size_t commits = 0;
    std::size_t merges = 0;
    std::optional<std::uint64_t> pull_request;
    std::string note;
    std::vector<finding> findings;
};

/**
 * @brief What compass check --ratchet found.
 *
 * base is the base as named, or the default it fell back to, and
 * merge_base the merge base of HEAD and it. current is the check of the
 * checkout and previous the check of the merge base, with the same
 * configuration. renames are the files of the merge base with findings
 * that HEAD holds at another path. split holds the findings of current
 * that previous did not have. provenance is what checking the commits of
 * the branch and its pull request found, when commit_provenance is on;
 * its findings are among those of split. blame is what blaming the
 * inherited findings of split found.
 */
struct ratchet_result {
    std::string base;
    store::types::object_id merge_base;
    check_result current;
    check_result previous;
    std::vector<rename> renames;
    ratchet_split split;
    provenance_result provenance;
    blame_report blame;
    double ms = 0;

    [[nodiscard]] bool failed() const noexcept {
        return current.run.failed() || previous.run.failed();
    }
};

/**
 * @brief What compass gc found and removed in the store whose packs are in
 * packs.
 */
struct gc_result {
    std::filesystem::path packs;
    store::types::collection_options options;
    store::types::collection_result collection;
};

}

#endif

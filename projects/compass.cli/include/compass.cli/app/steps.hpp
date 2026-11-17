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
#ifndef COMPASS_CLI_APP_STEPS_HPP
#define COMPASS_CLI_APP_STEPS_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.cli/io/listing.hpp"
#include "compass.codec/io/action_cache.hpp"
#include "compass.codec/io/dependency_file.hpp"
#include "compass.codec/io/output_sink.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/checkout_files.hpp"
#include "compass.codec/types/corpus.hpp"
#include "compass.codec/types/output_set.hpp"
#include "compass.codec/types/registry.hpp"
#include "compass.codec/types/run_result.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.store/io/git_objects.hpp"
#include "compass.store/io/id_resolver.hpp"
#include "compass.store/io/pack_store.hpp"
#include "compass.store/types/collection.hpp"
#include "compass.store/types/git_object.hpp"

/**
 * The steps of the pipeline, in the order the commands call them: list the
 * org files, build the type registry, resolve each file's ID, load each
 * file's object from the store or parse it, gather the loaded files into a
 * corpus, run the codecs, then store what later runs reuse and write or
 * diff the outputs. collect is the step of gc. Each step is one function,
 * so a caller can time it; none of them prints.
 */
namespace compass::cli::app {

/**
 * @brief A commit of the repository of a checkout, read from git's object
 * database with no checkout of it: its ID, the database and every path
 * the commit holds. Neither copied nor moved, since views point into it.
 */
struct commit_source {
    /**
     * @brief Opens the object database of the repository whose git common
     * directory is @p git_common_dir and lists the commit @p id.
     * @throws std::runtime_error when the commit or one of its trees
     * cannot be read, and as store::io::git_objects does.
     */
    commit_source(const std::filesystem::path& git_common_dir,
        const store::types::object_id& id);

    commit_source(const commit_source&) = delete;
    commit_source& operator=(const commit_source&) = delete;

    store::io::git_objects objects;
    store::types::object_id id;
    store::types::commit_files files;
};

/**
 * @brief The org files of a checkout or of a commit, one row per path,
 * and what resolving and loading them found.
 *
 * paths views strings the caller keeps alive. packs is the directory of
 * the store's packs, which store reads and store_objects writes to.
 * commit, when set, is the commit
 * the rows are read from instead of the checkout: their IDs are the ones
 * its trees record, their bytes its blobs, and there is no resolver. A
 * row whose error is not empty did not load and is left out of everything
 * after. A row loaded from the store has loaded set and its object in a
 * pack store maps; a row parsed instead has it in encoded, which tables points into, and which
 * store_objects writes. texts views each row's bytes: a file of the
 * checkout mapped in sources, or a blob of the commit held in blobs. Both
 * live as long as the files, since the corpus points into them.
 */
/**
 * @brief The content IDs of the listed files of a checkout, each resolved
 * at most once, on whichever thread asks first, as many codecs ask for the
 * ID of one file and each answer is an lstat or a hash. paths is in byte
 * order; ids holds a row's ID once it is resolved, or nothing when the
 * file could not be read.
 */
struct listed_ids {
    explicit listed_ids(std::vector<std::string_view> p)
        : paths(std::move(p)), once(paths.size()), ids(paths.size()) {}

    std::vector<std::string_view> paths;
    std::vector<std::once_flag> once;
    std::vector<std::optional<store::io::resolved_id>> ids;
};

struct corpus_files {
    std::filesystem::path checkout;
    std::filesystem::path packs;
    std::optional<store::io::pack_reader> store;
    const commit_source* commit = nullptr;
    std::vector<std::string_view> paths;
    std::optional<store::io::id_resolver> resolver;
    std::vector<store::io::resolved_id> resolved;
    std::vector<std::optional<platform::filesystem::mapped_file>> sources;
    std::vector<std::string> blobs;
    std::vector<std::string_view> texts;
    std::vector<std::uint8_t> loaded;
    std::vector<std::vector<std::byte>> encoded;
    std::vector<org::types::tables_view> tables;
    std::vector<std::string> errors;
    std::shared_ptr<listed_ids> listed;
};

/**
 * @brief Every file of a checkout, in byte order, and the org documents
 * among them, which view files.
 */
struct listing {
    std::vector<std::string> files;
    std::vector<std::string_view> documents;

    listing() = default;
    listing(const listing&) = delete;
    listing& operator=(const listing&) = delete;
};

/**
 * @brief Lists into @p l the files of @p checkout as io::list_files lists
 * them and, as documents, its org files as io::org_files picks them,
 * without the repository file.
 * @throws as io::list_files does.
 */
void list(listing& l, const std::filesystem::path& checkout);

/**
 * @brief Lists into @p l the files and documents of @p r, as the check
 * that wrote the listing record listed them, without starting git.
 */
void list(listing& l, io::recorded_listing r);

/**
 * @brief Lists into @p l every path of the commit @p c and, as documents,
 * its regular files whose name ends in .org, without the repository file.
 */
void list(listing& l, const commit_source& c);

/**
 * @brief The files of @p l as the codecs read them: for a checkout,
 * content IDs from the resolver of @p f and bytes from disk; for a
 * commit, the IDs its trees record and the bytes of its blobs, read only
 * when asked for. For a checkout, the IDs are resolved once each and
 * kept in @p f's listed.
 * @pre resolve has run on @p f. @p f and @p l outlive the result.
 */
[[nodiscard]] codec::types::checkout_files checkout_files_of(
    corpus_files& f, const listing& l);

/**
 * @brief The type registry the contract pages document_type_*.org under
 * @p meta declare, read in name order, with the model types after them.
 *
 * @throws std::system_error or std::filesystem::filesystem_error when the
 * directory or a page cannot be read.
 */
[[nodiscard]] org::types::type_registry build_type_registry(
    const std::filesystem::path& meta);

/**
 * @brief The type registry the contract pages document_type_*.org in the
 * directory @p meta of the commit @p c declare, read as the other
 * overload reads them from disk. @p meta is relative to the root of the
 * commit.
 *
 * @throws std::runtime_error when the commit has no directory @p meta or
 * a page's blob cannot be read.
 */
[[nodiscard]] org::types::type_registry build_type_registry(
    const commit_source& c, const std::filesystem::path& meta);

/**
 * @brief Rows for @p paths of @p checkout, or of @p commit when it is not
 * null, with nothing resolved or loaded, whose objects live in the packs
 * of @p packs, opened as they are now.
 */
[[nodiscard]] corpus_files open_files(const std::filesystem::path& checkout,
    const std::filesystem::path& packs,
    std::vector<std::string_view> paths,
    const commit_source* commit = nullptr);

/**
 * @brief Resolves the ID of every row. For a checkout, builds its resolver
 * and takes each ID from git's index, the worktree index or by hashing
 * the file, on up to @p threads threads; a file that cannot be hashed
 * gets an error. For a commit, takes the ID its trees record.
 */
void resolve(corpus_files& f, unsigned threads);

/**
 * @brief Loads every resolved row on up to @p threads threads: maps its
 * source, or reads its blob from the commit, and takes its object from the
 * store or, when it is missing or rejected, parses the source into an
 * object held in memory. Writes nothing. A file that cannot be read gets
 * an error.
 */
void load(corpus_files& f, unsigned threads);

/**
 * @brief The rows of @p f that loaded, as a corpus, each typed by its
 * #+type: against @p types. Points into @p f.
 */
[[nodiscard]] codec::types::corpus corpus_of(const corpus_files& f,
    const org::types::type_registry& types);

/**
 * @brief The dependency record of @p checkout, which run_codecs starts
 * from.
 */
[[nodiscard]] codec::io::loaded_record read_dependency_record(
    const std::filesystem::path& checkout);

/**
 * @brief Runs @p codecs over @p corpus on up to @p threads threads,
 * starting from @p previous and the result cache of the repository of
 * @p checkout, only the units @p only takes when given, checking the
 * inputs at @p edited first. Writes nothing.
 */
[[nodiscard]] codec::types::run_result run_codecs(
    const codec::types::registry& codecs, const codec::types::corpus& corpus,
    const org::types::type_registry& types, unsigned threads,
    const codec::types::dependency_record& previous,
    const std::filesystem::path& checkout,
    const codec::ops::unit_filter& only = {}, std::string_view edited = {});

/**
 * @brief Writes the object of every row that loaded by parsing to the
 * store, in one pack, and returns how many were written.
 *
 * @throws std::system_error when an object cannot be written.
 */
std::uint64_t store_objects(const corpus_files& f);

/**
 * @brief Replaces the worktree index with the rows that loaded and the
 * listed files whose IDs were resolved without git's index, and returns
 * how many entries it holds.
 *
 * @throws std::filesystem::filesystem_error when it cannot be written.
 */
std::size_t write_worktree_index(const corpus_files& f);

/**
 * @brief Puts the units of @p run that ran in the result cache of the
 * repository of @p checkout, on up to @p threads threads.
 */
codec::io::action_writes store_results(const std::filesystem::path& checkout,
    const codec::types::run_result& run, unsigned threads);

/**
 * @brief Replaces the dependency record of @p checkout with the one of
 * @p run when it differs, and returns true when it was written. A run
 * whose record is the previous one writes nothing and encodes nothing.
 */
bool write_dependency_record(const std::filesystem::path& checkout,
    const codec::types::run_result& run);

/**
 * @brief What writing the outputs did: sink, the files whose rewrite
 * deleted what may be code written by hand, why the written-outputs
 * record read was not used when it was not, and whether the record was
 * rewritten.
 */
struct sink_outcome {
    codec::io::sink_result sink;
    std::vector<codec::io::hand_edit> hand_edits;
    std::string record_problem;
    bool record_written = false;
};

/**
 * @brief Makes the files under @p root hold what @p codecs emitted in
 * @p run, touching only the files that differ, starting from the
 * written-outputs record of @p checkout. The record is rewritten only when
 * the sink had no problem.
 */
sink_outcome write_outputs(const std::filesystem::path& checkout,
    const std::filesystem::path& root, const codec::types::registry& codecs,
    const codec::types::run_result& run);

/**
 * @brief What writing the outputs would do, with the outputs and the
 * written-outputs record the plan points into, and why that record was
 * not used when it was not. Neither copied nor moved, so the plan's views
 * stay valid.
 */
struct planned_outputs {
    codec::types::output_set outputs;
    codec::io::loaded_outputs previous;
    codec::io::output_plan plan;

    planned_outputs() = default;
    planned_outputs(const planned_outputs&) = delete;
    planned_outputs& operator=(const planned_outputs&) = delete;
};

/**
 * @brief Plans into @p p writing what @p codecs emitted in @p run under
 * @p root, from the written-outputs record of @p checkout, as
 * write_outputs would. Writes nothing.
 */
void plan_outputs(planned_outputs& p, const std::filesystem::path& checkout,
    const std::filesystem::path& root, const codec::types::registry& codecs,
    const codec::types::run_result& run);

/**
 * @brief Collects the store of the repository @p checkout
 * belongs to, keeping every object a worktree's index names and every
 * action object a worktree's dependency record names.
 *
 * @throws std::runtime_error when the checkout is not a git checkout or an
 * index cannot be read, before anything is removed.
 */
[[nodiscard]] store::types::collection_result collect(
    const std::filesystem::path& checkout,
    const store::types::collection_options& options);

}

#endif

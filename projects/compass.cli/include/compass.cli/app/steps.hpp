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
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/io/action_cache.hpp"
#include "compass.codec/io/dependency_file.hpp"
#include "compass.codec/io/output_sink.hpp"
#include "compass.codec/types/checkout_files.hpp"
#include "compass.codec/types/corpus.hpp"
#include "compass.codec/types/output_set.hpp"
#include "compass.codec/types/registry.hpp"
#include "compass.codec/types/run_result.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.store/io/id_resolver.hpp"
#include "compass.store/types/collection.hpp"

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
 * @brief The org files of a checkout, one row per path, and what resolving
 * and loading them found.
 *
 * paths views strings the caller keeps alive. objects is the store the
 * objects are mapped from and written to. A row whose error is not empty
 * did not load and is left out of everything after. A row loaded from the
 * store has its object in mapped; a row parsed instead has it in encoded,
 * which tables points into, and which store_objects writes. sources stay
 * mapped while the files live, since the corpus points into them.
 */
struct corpus_files {
    std::filesystem::path checkout;
    std::filesystem::path objects;
    std::vector<std::string_view> paths;
    std::optional<store::io::id_resolver> resolver;
    std::vector<store::io::resolved_id> resolved;
    std::vector<std::optional<platform::filesystem::mapped_file>> sources;
    std::vector<std::optional<platform::filesystem::mapped_file>> mapped;
    std::vector<std::vector<std::byte>> encoded;
    std::vector<org::types::tables_view> tables;
    std::vector<std::string> errors;
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
 * @brief The files of @p l as the codecs read them: content IDs from the
 * resolver of @p f, bytes from disk.
 * @pre resolve has run on @p f. @p f and @p l outlive the result.
 */
[[nodiscard]] codec::types::checkout_files checkout_files_of(
    const corpus_files& f, const listing& l);

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
 * @brief Rows for @p paths of @p checkout, with nothing resolved or loaded,
 * whose objects live under @p objects.
 */
[[nodiscard]] corpus_files open_files(const std::filesystem::path& checkout,
    const std::filesystem::path& objects,
    std::vector<std::string_view> paths);

/**
 * @brief Builds the resolver of the checkout and resolves the ID of every
 * row from git's index, the worktree index or by hashing the file, on up
 * to @p threads threads. A file that cannot be hashed gets an error.
 */
void resolve(corpus_files& f, unsigned threads);

/**
 * @brief Loads every resolved row on up to @p threads threads: maps its
 * source, and maps its object from the store or, when it is missing or
 * rejected, parses the source into an object held in memory. Writes
 * nothing. A file that cannot be read gets an error.
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
 * @p checkout. Writes nothing.
 */
[[nodiscard]] codec::types::run_result run_codecs(
    const codec::types::registry& codecs, const codec::types::corpus& corpus,
    const org::types::type_registry& types, unsigned threads,
    const codec::types::dependency_record& previous,
    const std::filesystem::path& checkout);

/**
 * @brief Writes the object of every row that loaded by parsing to the
 * store, and returns how many were written.
 *
 * @throws std::system_error when an object cannot be written.
 */
std::uint64_t store_objects(const corpus_files& f);

/**
 * @brief Replaces the worktree index with the rows that loaded, and
 * returns how many entries it holds.
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
 * @p run when it differs, and returns true when it was written.
 */
bool write_dependency_record(const std::filesystem::path& checkout,
    const codec::types::run_result& run);

/**
 * @brief What writing the outputs did: sink, why the written-outputs
 * record read was not used when it was not, and whether the record was
 * rewritten.
 */
struct sink_outcome {
    codec::io::sink_result sink;
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
 * @brief Collects the store at @p objects of the repository @p checkout
 * belongs to, keeping every object a worktree's index names and every
 * action object a worktree's dependency record names.
 *
 * @throws std::runtime_error when the checkout is not a git checkout or an
 * index cannot be read, before anything is removed.
 */
[[nodiscard]] store::types::collection_result collect(
    const std::filesystem::path& checkout,
    const std::filesystem::path& objects,
    const store::types::collection_options& options);

}

#endif

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
#include "compass.cli/app/pipeline.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "compass.cli/app/steps.hpp"
#include "compass.cli/app/codecs.hpp"
#include "compass.cli/config/repository_file.hpp"
#include "compass.cli/io/history.hpp"
#include "compass.cli/io/listing.hpp"
#include "compass.cli/ops/attribution.hpp"
#include "compass.cli/ops/provenance.hpp"
#include "compass.cli/app/blame.hpp"
#include "compass.cli/app/codecs.hpp"
#include "compass.cli/ops/ratchet.hpp"
#include "compass.codec/io/action_cache.hpp"
#include "compass.codec/io/dependency_file.hpp"
#include "compass.codec/io/format_cache.hpp"
#include "compass.codec/io/unmarked_file.hpp"
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/ops/dependencies.hpp"
#include "compass.codec/ops/exceptions.hpp"
#include "compass.codec/ops/hand_edits.hpp"
#include "compass.codec/ops/outputs.hpp"
#include "compass.codec/ops/parallel_for.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/codec_failure.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.store/io/collection.hpp"
#include "compass.store/io/git_objects.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/ops/git_index.hpp"

namespace compass::cli::app {

namespace {

namespace ct = compass::codec::types;

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

/**
 * @brief Everything a run of the pipeline read and computed, before
 * anything is written. The files, the checkout's files and the corpus
 * point into listing and, for a commit, into commit, so the pipeline is
 * never moved once evaluated. overlaid holds the documents diff parsed
 * from the outputs of an earlier pass, which the corpus then points into.
 */
struct pipeline {
    std::optional<commit_source> commit;
    org::types::type_registry types;
    listing listed;
    corpus_files files;
    ct::checkout_files checkout;
    ct::corpus corpus;
    ct::registry codecs;
    codec::io::loaded_record previous;
    ct::run_result run;
    std::deque<org::types::tables> overlaid;
    types::run_summary summary;

    /**
     * @brief Runs @p step and records its wall time as @p name.
     */
    template <typename Step>
    void timed(std::string_view name, Step&& step) {
        const auto start = std::chrono::steady_clock::now();
        step();
        summary.steps.emplace_back(name, since(start));
    }
};

/**
 * @brief @p path, a directory under @p checkout, relative to the root of
 * a commit of it.
 * @throws std::runtime_error when @p path is outside the checkout, so
 * the commit has no such directory.
 */
std::filesystem::path in_commit(const std::filesystem::path& path,
    const std::filesystem::path& checkout) {
    auto relative = path.lexically_relative(checkout);
    if (relative.empty() || *relative.begin() == "..")
        throw std::runtime_error(path.generic_string() +
            " is outside the checkout, so a commit cannot hold it");
    return relative == "." ? std::filesystem::path{} : relative;
}

/**
 * @brief Lists the files of the checkout of @p o, or of the commit it
 * names, into @p p, and builds the type and codec registries. When
 * @p recorded, a checkout is listed from the listing record the last check
 * wrote, if there is one, without listing the checkout: @p created, a file
 * that may be new, is added to it when git lists that one path. Writes
 * nothing.
 */
void list_files(pipeline& p, const types::options& o,
    bool recorded = false, std::string_view created = {}) {
    if (o.commit)
        p.timed("tree", [&] {
            p.commit.emplace(store::io::git_common_dir(o.checkout),
                *o.commit);
        });
    const commit_source* commit = p.commit ? &*p.commit : nullptr;

    p.timed("types", [&] {
        p.types = commit
            ? build_type_registry(*commit, in_commit(o.meta, o.checkout))
            : build_type_registry(o.meta);
    });

    p.timed("list", [&] {
        if (commit) {
            list(p.listed, *commit);
            return;
        }
        if (recorded) {
            if (auto r = io::read_listing_file(io::listing_file_path(
                    store::io::git_dir(o.checkout)))) {
                const auto at = std::ranges::lower_bound(r->files, created);
                if (!created.empty() &&
                    (at == r->files.end() || *at != created)) {
                    const auto one = io::list_files(o.checkout, created);
                    if (std::ranges::contains(one, created)) {
                        const bool document = created !=
                            config::repository_file_name &&
                            !io::org_files(o.checkout, one).empty();
                        r->documents.insert(r->documents.begin() +
                            (at - r->files.begin()), document ? 1 : 0);
                        r->files.insert(at, std::string(created));
                    }
                }
                list(p.listed, std::move(*r));
                return;
            }
        }
        list(p.listed, o.checkout);
    });
    std::vector<codec::types::descriptor> all(codecs().begin(),
        codecs().end());
    if (o.checks)
        for (const auto& c : o.checks->checks)
            all.push_back(c->codec);
    p.codecs = codec::ops::make_registry(all, p.types, o.settings);
    p.summary.problems = p.codecs.problems;
}

/**
 * @brief Resolves and loads the documents listed in @p p, or none when
 * not @p with_documents, into its corpus, which sees every listed file.
 * Reads the checkout or git's objects and the store, and writes nothing.
 */
void load_files(pipeline& p, const types::options& o,
    bool with_documents = true) {
    auto& s = p.summary;
    const auto documents = with_documents ? p.listed.documents
                                          : std::vector<std::string_view>{};
    p.files = open_files(o.checkout,
        store::io::packs_dir(store::io::git_common_dir(o.checkout)),
        documents, p.commit ? &*p.commit : nullptr);
    s.files = documents.size();
    p.timed("resolve", [&] { resolve(p.files, o.threads); });
    p.timed("load", [&] { load(p.files, o.threads); });

    for (std::size_t row = 0; row < documents.size(); ++row) {
        if (!p.files.errors[row].empty())
            s.errors.push_back(std::string(documents[row]) + ": " +
                p.files.errors[row]);
        else
            ++(p.files.loaded[row] != 0 ? s.loaded : s.parsed);
    }
    p.checkout = checkout_files_of(p.files, p.listed);
    p.corpus = corpus_of(p.files, p.types);
    p.corpus.files = &p.checkout;
    s.documents = p.corpus.size();
}

/**
 * @brief Runs the codecs of @p p from @p previous and the result cache,
 * the units @p only takes when given, checking the inputs at @p edited
 * first, and records what each codec ran.
 */
void run_units(pipeline& p, const types::options& o,
    const ct::dependency_record& previous,
    const codec::ops::unit_filter& only = {}, std::string_view edited = {}) {
    auto& s = p.summary;
    p.timed("codecs", [&] {
        p.run = run_codecs(p.codecs, p.corpus, p.types, o.threads, previous,
            o.checkout, only, edited);
    });
    for (std::size_t c = 0; c < p.codecs.size(); ++c)
        s.codecs.emplace_back(p.codecs.codecs[c].name);
    s.ran = p.run.ran;
    s.skipped = p.run.skipped;
    s.cached = p.run.cached;
    s.codec_ms = p.run.ms;
    s.formats.resize(p.codecs.size());
}

/**
 * @brief Lists, loads and runs the codecs over the checkout of @p o, or
 * the commit it names, into @p p. Reads the checkout or git's objects,
 * the store, the dependency record and the result cache, and writes
 * nothing.
 */
void evaluate(pipeline& p, const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    list_files(p, o);
    load_files(p, o);
    p.timed("record", [&] {
        p.previous = read_dependency_record(o.checkout);
    });
    run_units(p, o, p.previous.record);
    p.summary.ms = since(start);
}

/**
 * @brief Formats the files of the codecs of @p p that name formatters,
 * through the result cache of the repository of @p o's checkout. Nothing
 * is formatted when the run failed.
 */
void format(pipeline& p, const types::options& o) {
    if (p.summary.failed())
        return;
    p.timed("format", [&] {
        p.summary.formats = codec::io::format_outputs(p.codecs, p.run,
            o.checkout, store::io::git_common_dir(o.checkout), o.threads);
    });
}

/**
 * @brief Writes what @p p computed that later runs reuse: the objects it
 * parsed and the result cache, which every worktree shares, and for a
 * checkout its worktree index and dependency record, with the keys the
 * new record retired. A run over a commit leaves the records of the
 * checkout as they are, since they describe its files. Then collects the
 * store when it has grown enough, as store::io::collect_when_grown says;
 * a collection that fails is reported and fails nothing.
 */
void persist(pipeline& p, unsigned threads) {
    p.timed("persist", [&] {
        store_objects(p.files);
        store_results(p.files.checkout, p.run, threads);
        if (p.commit)
            return;
        write_worktree_index(p.files);
        if (write_dependency_record(p.files.checkout, p.run))
            codec::io::retire_keys(store::io::git_dir(p.files.checkout),
                p.previous.record, p.run.record, p.run.fresh);
        io::write_listing_file(io::listing_file_path(
            store::io::git_dir(p.files.checkout)), p.listed.files,
            p.listed.documents);
    });
    p.timed("collect", [&] {
        try {
            p.summary.collection = store::io::collect_when_grown(
                p.files.checkout, codec::io::action_keys);
        } catch (const std::exception& e) {
            p.summary.collection_problem = e.what();
        }
    });
}

/**
 * @brief The 1-based line and byte column of @p offset in @p source.
 */
std::pair<std::uint32_t, std::uint32_t> position_of(std::string_view source,
    std::uint32_t offset) {
    const auto before = source.substr(0, offset);
    const auto line = 1 + std::ranges::count(before, '\n');
    const auto newline = before.rfind('\n');
    const auto column = newline == std::string_view::npos
        ? before.size() + 1 : before.size() - newline;
    return {static_cast<std::uint32_t>(line),
        static_cast<std::uint32_t>(column)};
}

/**
 * @brief The bytes of the file at @p path, or nothing when it cannot be
 * read.
 */
std::optional<std::string> read_bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(in), {});
}

/**
 * @brief The drift finding of @p codec for the file at @p path, whose
 * bytes are @p source, located at byte @p at of them, with its @p sides.
 */
types::finding drift_finding(std::string path, std::string_view source,
    std::uint32_t at, const std::string& codec, std::string message,
    types::drift_sides sides) {
    const auto [line, column] = position_of(source, at);
    return {
        .path = std::move(path),
        .line = line,
        .column = column,
        .rule = std::format("{}/{}", codec, codec::ops::drift_rule.name),
        .level = codec::types::severity::error,
        .message = std::move(message),
        .fix = "run compass generate, or change the model or template "
               "that generates it",
        .drift = std::move(sides),
        .subject = {},
        .subject_text = {}};
}

/**
 * @brief One finding for each file that writing the outputs of @p plan
 * under @p root would change, located at the first byte that differs in
 * the file on disk. Its path is relative to @p checkout when the file is
 * under it.
 */
std::vector<types::finding> drift_of(const codec::io::output_plan& plan,
    const std::filesystem::path& root,
    const std::filesystem::path& checkout) {
    using codec::io::output_action;
    std::vector<types::finding> out;
    for (std::size_t i = 0; i < plan.size(); ++i) {
        const auto action = plan.actions[i];
        if (!codec::io::changes_file(action))
            continue;
        const auto file = root / plan.paths[i];
        const auto disk = read_bytes(file);
        const std::string codec(plan.codecs[i]);
        std::string message;
        std::uint32_t at = 0;
        types::drift_sides sides;
        if (disk)
            sides.file = store::ops::blob_id_of(*disk);
        if (action != output_action::remove)
            sides.generated = store::ops::blob_id_of(plan.contents[i]);
        if (action == output_action::remove) {
            message = "compass wrote this file and " + codec +
                " no longer generates it";
        } else if (!disk) {
            message = codec + " generates this file and it is missing";
        } else {
            const auto wanted = plan.contents[i];
            at = static_cast<std::uint32_t>(std::ranges::mismatch(*disk,
                wanted).in1 - disk->begin());
            message = action == output_action::restore
                ? "the file was edited after compass wrote it"
                : "the file differs from what " + codec + " generates";
        }
        const auto relative = file.lexically_relative(checkout);
        out.push_back(drift_finding(relative.empty() ||
                *relative.begin() == ".." ? file.generic_string()
                                          : relative.generic_string(),
            disk.value_or(""), at, codec, std::move(message),
            std::move(sides)));
    }
    return out;
}

/**
 * @brief Each script, a word ending in .py or .sh, that the text @p text
 * names, with its offset: a path when it holds a slash, a file name
 * otherwise.
 */
std::vector<std::pair<std::string_view, std::uint32_t>> scripts_named(
    std::string_view text) {
    const auto word = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' ||
            c == '.' || c == '/' || c == '-';
    };
    std::vector<std::pair<std::string_view, std::uint32_t>> out;
    for (const std::string_view ext : {".py", ".sh"}) {
        for (auto at = text.find(ext); at != std::string_view::npos;
             at = text.find(ext, at + 1)) {
            const auto end = at + ext.size();
            if (end < text.size() && (std::isalnum(static_cast<unsigned char>(
                    text[end])) || text[end] == '_'))
                continue;
            auto begin = at;
            while (begin > 0 && word(text[begin - 1]))
                --begin;
            while (begin < at && (text[begin] == '.' || text[begin] == '/'))
                ++begin;
            if (begin == at)
                continue;
            out.emplace_back(text.substr(begin, end - begin),
                static_cast<std::uint32_t>(begin));
        }
    }
    return out;
}

/**
 * @brief One finding for each script the header of a file of @p outputs,
 * its first codec::ops::generated_header_lines lines, names and the
 * checkout or commit, listed in @p files, lacks: a path must be one of
 * them, a file name the name of one. A generated banner that names a
 * deleted script sends its reader nowhere.
 */
std::vector<types::finding> names_missing_of(
    const codec::types::output_set& outputs,
    std::span<const std::string> files) {
    std::set<std::string_view> names;
    for (const auto& f : files) {
        const auto slash = f.rfind('/');
        names.insert(slash == std::string::npos ? std::string_view(f)
            : std::string_view(f).substr(slash + 1));
    }
    std::vector<types::finding> out;
    for (std::uint32_t i = 0; i < outputs.size(); ++i) {
        const codec::types::file_id id{i};
        const auto whole = outputs.files.contents[id];
        auto header_end = std::string_view::size_type{0};
        for (std::uint32_t l = 0; l < codec::ops::generated_header_lines &&
             header_end != std::string_view::npos; ++l) {
            header_end = whole.find('\n', header_end);
            if (header_end != std::string_view::npos)
                ++header_end;
        }
        const auto text = whole.substr(0, header_end);
        for (const auto& [script, at] : scripts_named(text)) {
            const bool path = script.find('/') != std::string_view::npos;
            const bool held = path
                ? std::ranges::binary_search(files, script)
                : names.contains(script);
            if (held)
                continue;
            const auto [line, column] = position_of(text, at);
            out.push_back({.path = std::string(outputs.files.paths[id]),
                .line = line,
                .column = column,
                .rule = std::format("{}/{}", outputs.codecs[id],
                    codec::ops::names_missing_file_rule.name),
                .level = codec::types::severity::error,
                .message = std::format("{} generates this file naming {}, "
                    "which the checkout does not hold", outputs.codecs[id],
                    script),
                .fix = "change the codec so it names a file the project "
                       "has, or restore the script",
                .drift = {},
                .subject = {},
                .subject_text = {}});
        }
    }
    return out;
}

/**
 * @brief One finding for each output of @p outputs that the commit @p c
 * does not hold as it is generated, under @p root, relative to the root
 * of the commit: what drift_of finds in a fresh checkout of the commit,
 * which has no written-outputs record, so nothing is removed or restored.
 * A blob is read only when its ID differs from the output's.
 */
std::vector<types::finding> commit_drift_of(const commit_source& c,
    const codec::types::output_set& outputs,
    const std::filesystem::path& root) {
    std::vector<types::finding> out;
    for (std::uint32_t i = 0; i < outputs.size(); ++i) {
        const codec::types::file_id id{i};
        const auto path = (root / outputs.files.paths[id]).generic_string();
        const auto row = c.files.find(path);
        if (row && c.files.ids[*row] == outputs.hashes[id])
            continue;
        const std::string codec(outputs.codecs[id]);
        std::optional<store::types::git_object> blob;
        if (row && c.files.modes[*row] != store::types::gitlink_mode)
            blob = c.objects.read(c.files.ids[*row]);
        types::drift_sides sides{.file = {}, .generated = outputs.hashes[id]};
        if (!blob) {
            out.push_back(drift_finding(path, "", 0, codec,
                codec + " generates this file and it is missing", sides));
            continue;
        }
        sides.file = c.files.ids[*row];
        const auto wanted = outputs.files.contents[id];
        const auto at = static_cast<std::uint32_t>(std::ranges::mismatch(
            blob->bytes, wanted).in1 - blob->bytes.begin());
        out.push_back(drift_finding(path, blob->bytes, at, codec,
            "the file differs from what " + codec + " generates", sides));
    }
    return out;
}

/**
 * @brief The files with @p findings at the commit @p from that the
 * commit @p to holds at another path: those whose blob it holds at a new
 * path, then those git's rename detection pairs by similarity. git runs
 * only when a file with findings is gone and no blob explains it.
 */
std::vector<types::rename> renames_of(const std::filesystem::path& checkout,
    const store::types::object_id& from, const store::types::object_id& to,
    const std::vector<types::finding>& findings) {
    const store::io::git_objects db(store::io::git_objects_dir(
        store::io::git_common_dir(checkout)));
    const auto head = store::io::list_commit(db, to);
    std::vector<std::string_view> gone;
    for (const auto& f : findings) {
        if (!head.find(f.path))
            gone.push_back(f.path);
    }
    if (gone.empty())
        return {};
    const auto base = store::io::list_commit(db, from);
    std::erase_if(gone, [&](std::string_view p) { return !base.find(p); });
    std::ranges::sort(gone);
    gone.erase(std::unique(gone.begin(), gone.end()), gone.end());
    auto out = ops::exact_renames(base, head, gone);
    if (out.size() == gone.size())
        return out;
    for (auto& r : io::similar_renames(checkout, from, to)) {
        const bool wanted = std::ranges::binary_search(gone, r.from);
        const bool known = std::ranges::contains(out, r.from,
            &types::rename::from);
        if (wanted && !known)
            out.push_back(std::move(r));
    }
    return out;
}

/**
 * @brief The most passes generate and diff make. A pass runs again only
 * when it changed a file a codec of it read, so a run that needs more has
 * outputs that feed their own inputs.
 */
constexpr std::uint32_t max_passes = 4;

/**
 * @brief A file of the checkout a pass changes, by path relative to the
 * checkout, and whether the change creates or removes it, which changes
 * the listings that match it.
 */
struct file_change {
    std::string path;
    bool listing = false;
};

/**
 * @brief What the codecs of a run read, from its record: the paths of the
 * files and documents read or looked up, the listing patterns, and
 * whether any read a set of documents, looked up an ID or read the type
 * registry, which a document anywhere may change.
 */
struct run_reads {
    std::unordered_set<std::string_view> paths;
    std::vector<std::string_view> listings;
    bool documents = false;
};

run_reads reads_of(const ct::dependency_record& record) {
    run_reads out;
    for (std::uint32_t i = 0; i < record.input_count(); ++i) {
        const ct::input_id in{i};
        const auto key = record.input_keys[in];
        switch (record.input_kinds[in]) {
        case ct::input_kind::path:
        case ct::input_kind::file:
        case ct::input_kind::file_exists:
            out.paths.insert(key);
            break;
        case ct::input_kind::listing:
            out.listings.push_back(key);
            break;
        case ct::input_kind::id:
        case ct::input_kind::type_set:
        case ct::input_kind::type_registry:
            out.documents = true;
            break;
        case ct::input_kind::fact:
            break;
        }
    }
    return out;
}

bool is_document(std::string_view path) {
    return path.ends_with(".org") && path != config::repository_file_name;
}

/**
 * @brief Whether @p c changes an answer a codec that read @p r got.
 */
bool feeds(const run_reads& r, const file_change& c) {
    if (r.paths.contains(c.path) || (r.documents && is_document(c.path)))
        return true;
    if (!c.listing)
        return false;
    return std::ranges::any_of(r.listings, [&](std::string_view pattern) {
        return ops::matches_listing(pattern, c.path);
    });
}

/**
 * @brief The paths of @p changes that change what a codec of @p run read,
 * in order.
 */
std::vector<std::string> fed_back(const ct::run_result& run,
    std::span<const file_change> changes) {
    std::vector<std::string> out;
    const auto reads = reads_of(run.record);
    for (const auto& c : changes) {
        if (feeds(reads, c))
            out.push_back(c.path);
    }
    return out;
}

/**
 * @brief @p path, relative to the output root of @p o, relative to its
 * checkout; none when it lies outside the checkout, where no codec reads.
 */
std::optional<std::string> checkout_path(const types::options& o,
    std::string_view path) {
    auto relative = (o.output_root / path).lexically_normal()
        .lexically_relative(o.checkout.lexically_normal());
    if (relative.empty() || *relative.begin() == "..")
        return std::nullopt;
    return relative.generic_string();
}

/**
 * @brief The files of the checkout that @p actions, what writing the
 * outputs of @p p did, by path relative to the output root, changed.
 */
std::vector<file_change> written_changes(const pipeline& p,
    const types::options& o,
    const std::vector<std::pair<std::string, codec::io::output_action>>&
        actions) {
    std::vector<file_change> out;
    for (const auto& [root_path, action] : actions) {
        if (!codec::io::changes_file(action))
            continue;
        if (auto path = checkout_path(o, root_path)) {
            const bool listing = action == codec::io::output_action::remove ||
                !ct::holds_file(&p.checkout, *path);
            out.push_back({std::move(*path), listing});
        }
    }
    return out;
}

/**
 * @brief A file of the checkout a planned pass changes, with the bytes it
 * leaves there, none when it removes the file.
 */
struct planned_change {
    file_change file;
    std::optional<std::string> content;
};

/**
 * @brief The files of the checkout that @p plan changes against the files
 * the codecs of @p p saw: those it writes with other bytes, and those it
 * removes that are listed. A pass that changes none the codecs read has
 * reached what generate converges to.
 */
std::vector<planned_change> planned_changes(const pipeline& p,
    const types::options& o, const codec::io::output_plan& plan) {
    using codec::io::output_action;
    std::vector<planned_change> out;
    for (std::size_t i = 0; i < plan.size(); ++i) {
        const auto action = plan.actions[i];
        if (action == output_action::keep)
            continue;
        auto path = checkout_path(o, plan.paths[i]);
        if (!path)
            continue;
        const bool listed = ct::holds_file(&p.checkout, *path);
        if (action == output_action::remove) {
            if (listed)
                out.push_back({{std::move(*path), true}, std::nullopt});
            continue;
        }
        const auto bytes = plan.contents[i];
        const auto now = listed ? p.checkout.content_of(*path) : std::nullopt;
        if (!now || *now != store::ops::blob_id_of(bytes))
            out.push_back({{std::move(*path), !listed}, std::string(bytes)});
    }
    return out;
}

/**
 * @brief Makes the files and documents the codecs of @p p read hold what
 * @p changes leave in the checkout, in memory, so a later run of the
 * codecs sees them as the next generate pass would read them from disk.
 */
void overlay(pipeline& p, std::vector<planned_change> changes) {
    using changed_map = std::map<std::string, std::optional<std::string>,
        std::less<>>;
    auto over = std::make_shared<changed_map>();
    for (auto& c : changes)
        (*over)[std::move(c.file.path)] = std::move(c.content);
    auto base = std::make_shared<const ct::checkout_files>(
        std::move(p.checkout));

    ct::checkout_files next;
    for (const auto path : base->paths) {
        const auto it = over->find(path);
        if (it == over->end() || it->second)
            next.paths.push_back(path);
    }
    for (const auto& [path, content] : *over) {
        if (content && !ct::holds_file(base.get(), path))
            next.paths.push_back(path);
    }
    std::ranges::sort(next.paths);
    next.content_of = [base, over](std::string_view path)
        -> std::optional<store::types::blob_id> {
        const auto it = over->find(path);
        if (it == over->end())
            return base->content_of(path);
        if (!it->second)
            return std::nullopt;
        return store::ops::blob_id_of(*it->second);
    };
    next.read = [base, over](std::string_view path)
        -> std::optional<std::string> {
        const auto it = over->find(path);
        if (it == over->end())
            return base->read(path);
        return it->second;
    };
    p.checkout = std::move(next);
    p.corpus.files = &p.checkout;

    auto& c = p.corpus;
    for (const auto& [path, content] : *over) {
        if (!is_document(path))
            continue;
        const auto at = std::ranges::lower_bound(c.paths, path);
        const auto row = at - c.paths.begin();
        const bool present = at != c.paths.end() && *at == path;
        if (present && !content) {
            c.paths.erase(c.paths.begin() + row);
            c.contents.erase(c.contents.begin() + row);
            c.sources.erase(c.sources.begin() + row);
            c.tables.erase(c.tables.begin() + row);
            c.types.erase(c.types.begin() + row);
        }
        if (!content)
            continue;
        auto& parsed = p.overlaid.emplace_back();
        ingest::org::parse(*content, parsed);
        const auto view = org::types::view(parsed);
        const auto id = store::ops::blob_id_of(*content);
        const auto type = codec::ops::document_type(*content, view, p.types);
        if (!present) {
            c.paths.insert(c.paths.begin() + row, path);
            c.contents.insert(c.contents.begin() + row, id);
            c.sources.insert(c.sources.begin() + row, *content);
            c.tables.insert(c.tables.begin() + row, view);
            c.types.insert(c.types.begin() + row, type);
            continue;
        }
        const auto u = static_cast<std::size_t>(row);
        c.contents[u] = id;
        c.sources[u] = *content;
        c.tables[u] = view;
        c.types[u] = type;
    }
}

/**
 * @brief Runs the codecs of @p p again over its corpus as overlay left
 * it, from @p previous and the result cache as evaluate ran them, adding
 * what they ran to its summary.
 */
void run_again(pipeline& p, const types::options& o,
    const ct::dependency_record& previous) {
    p.timed("codecs", [&] {
        p.run = run_codecs(p.codecs, p.corpus, p.types, o.threads,
            previous, o.checkout);
    });
    auto& s = p.summary;
    for (std::size_t c = 0; c < p.codecs.size(); ++c) {
        s.ran[c] += p.run.ran[c];
        s.skipped[c] += p.run.skipped[c];
        s.cached[c] += p.run.cached[c];
        s.codec_ms[c] += p.run.ms[c];
    }
}

void run_again(pipeline& p, const types::options& o) {
    run_again(p, o, p.previous.record);
}

/**
 * @brief Adds @p from, the summary of pass @p pass of generate, to @p to,
 * that of the passes before it. The documents are counted as the first
 * pass read them, and each later step is named with its pass.
 */
void add_pass(types::run_summary& to, types::run_summary from,
    std::uint32_t pass) {
    if (pass == 1) {
        to = std::move(from);
        return;
    }
    std::ranges::move(from.errors, std::back_inserter(to.errors));
    std::ranges::move(from.problems, std::back_inserter(to.problems));
    for (std::size_t c = 0; c < from.ran.size() && c < to.ran.size(); ++c) {
        to.ran[c] += from.ran[c];
        to.skipped[c] += from.skipped[c];
        to.cached[c] += from.cached[c];
        to.codec_ms[c] += from.codec_ms[c];
        const auto& f = from.formats[c];
        auto& into = to.formats[c];
        if (!f)
            continue;
        if (!into) {
            into = f;
            continue;
        }
        into->hits += f->hits;
        into->formatted += f->formatted;
        into->unformatted += f->unformatted;
        into->runs += f->runs;
        into->format_ms += f->format_ms;
    }
    for (auto& [name, ms] : from.steps)
        to.steps.emplace_back(std::format("{} (pass {})", name, pass), ms);
    if (from.collection)
        to.collection = std::move(from.collection);
    if (!from.collection_problem.empty())
        to.collection_problem = std::move(from.collection_problem);
}

/**
 * @brief The counter of @p sink that a file changed by @p a counts in.
 * @pre codec::io::changes_file(@p a).
 */
std::uint64_t& counter_of(codec::io::sink_result& sink,
    codec::io::output_action a) {
    using codec::io::output_action;
    if (a == output_action::write)
        return sink.written;
    return a == output_action::restore ? sink.restored : sink.removed;
}

/**
 * @brief Counts in @p sink, what the last pass of generate did, as
 * @p last says file by file, each file of @p earlier, which the passes
 * before it changed, by the last change they made to it, when the last
 * pass left it alone. Each file is then counted once, as what the whole
 * run did to it.
 */
void count_earlier(codec::io::sink_result& sink,
    const std::map<std::string, codec::io::output_action>& earlier,
    const std::vector<std::pair<std::string, codec::io::output_action>>&
        last) {
    using codec::io::output_action;
    std::unordered_map<std::string_view, output_action> now;
    for (const auto& [path, action] : last)
        now.emplace(path, action);
    for (const auto& [path, action] : earlier) {
        const auto it = now.find(path);
        if (it == now.end()) {
            ++counter_of(sink, action);
        } else if (it->second == output_action::unchanged) {
            --sink.unchanged;
            ++counter_of(sink, action);
        }
    }
}

/**
 * @brief The hand edits generate deleted, judged once all its passes are
 * done: the lines of each file in @p originals, as it was before generate
 * wrote it, that the file under @p root now lacks. A pass can delete lines
 * a later pass restores, as when it runs a template a later pass
 * regenerates, so a pass's own judgement is only a candidate.
 */
std::vector<codec::io::hand_edit> hand_edits_left(
    const std::filesystem::path& root,
    const std::map<std::string, std::string>& originals) {
    std::vector<codec::io::hand_edit> out;
    for (const auto& [path, original] : originals) {
        std::ifstream in(root / path, std::ios::binary);
        const std::string now(std::istreambuf_iterator<char>(in), {});
        if (auto block = codec::ops::dropped_hand_lines(original, now))
            out.push_back({path, std::move(*block)});
    }
    return out;
}

/**
 * @brief Why generate or diff gave up after max_passes passes, naming
 * @p paths, the files of the checkout the last pass still changed that a
 * codec read.
 */
std::string unsettled(const std::vector<std::string>& paths) {
    std::string names;
    for (const auto& path : paths)
        names += (names.empty() ? "" : ", ") + path;
    return std::format("the outputs did not settle in {} passes, as each "
        "pass changed files the codecs read, so an output feeds its own "
        "input; still changing: {}", max_passes, names);
}

/**
 * @brief The files of the checkout of @p o that git says changed since
 * HEAD, staged or not, and those it neither tracks nor ignores, relative
 * to the checkout.
 */
std::vector<std::string> changed_since_head(const types::options& o) {
    auto out = io::git_lines(o.checkout, {"-c", "core.quotePath=false",
        "diff", "--name-only", "--relative", "HEAD"});
    std::ranges::move(io::git_lines(o.checkout, {"-c",
        "core.quotePath=false", "ls-files", "--others", "--exclude-standard"}),
        std::back_inserter(out));
    return out;
}

/**
 * @brief The files of the checkout of @p p that its outputs change, with
 * the bytes they leave there, as planned_changes finds them, but against
 * the files the codecs saw rather than a plan: nothing on disk is read
 * and nothing is removed.
 */
std::vector<planned_change> generated_changes(const pipeline& p,
    const types::options& o) {
    std::vector<planned_change> out;
    const auto outputs = codec::ops::outputs_of(p.codecs, p.run);
    for (std::uint32_t row = 0; row < outputs.size(); ++row) {
        const ct::file_id f{row};
        auto path = checkout_path(o, outputs.files.paths[f]);
        if (!path)
            continue;
        const bool listed = ct::holds_file(&p.checkout, *path);
        const auto now = listed ? p.checkout.content_of(*path) : std::nullopt;
        if (!now || *now != outputs.hashes[f])
            out.push_back({{std::move(*path), !listed},
                std::string(outputs.files.contents[f])});
    }
    return out;
}

/**
 * @brief What HEAD generates, to tell a change the checkout made from
 * drift HEAD already had, found once, when a file first needs it.
 *
 * When git lists no change since HEAD, tracked or not, the checkout
 * generates what HEAD does, so every change is stale and HEAD is not
 * evaluated. When the configuration file or a check document changed,
 * the codecs themselves differ, and every change counts as the
 * checkout's. Otherwise HEAD is evaluated from git's objects, with the
 * settings of the checkout, through the dependency record and the result
 * cache, in as many passes as diff makes, and each output it generates is
 * kept with what HEAD's commit holds at its path.
 */
class head_comparison {
public:
    explicit head_comparison(const types::options& o) : o_(o) {}

    /**
     * @brief Asks git what changed since HEAD, once. generate calls it
     * before it writes, so its own writes do not count.
     */
    void observe() {
        if (observed_)
            return;
        observed_ = true;
        try {
            head_ = io::resolve_commit(o_.checkout, "HEAD");
        } catch (const std::runtime_error&) {
            state_ = state::none;
            note_ = "the checkout has no commit, so its changes cannot be "
                "told from drift";
            return;
        }
        const auto changed = changed_since_head(o_);
        const auto checks = std::string(config::checks_directory) + "/";
        if (changed.empty()) {
            state_ = state::clean;
        } else if (std::ranges::any_of(changed, [&](const std::string& p) {
                return p == config::repository_file_name ||
                    p.starts_with(checks);
            })) {
            state_ = state::reconfigured;
            note_ = "the configuration changed since HEAD, so every change "
                "counts as yours";
        }
    }

    /**
     * @brief Why the output at @p path, relative to the output root,
     * changes when the checkout generates @p generated there, none for a
     * file it no longer generates. HEAD runs from @p record.
     */
    types::change_cause cause(std::string_view path,
        const std::optional<store::types::blob_id>& generated,
        const ct::dependency_record& record) {
        observe();
        if (state_ == state::changed)
            evaluate(record);
        switch (state_) {
        case state::clean: return types::change_cause::stale;
        case state::reconfigured: return types::change_cause::yours;
        case state::none: return types::change_cause::unknown;
        case state::changed:
        case state::compared: break;
        }
        const auto it = files_.find(path);
        if (it == files_.end())
            return ops::cause_of(generated, std::nullopt, std::nullopt);
        return ops::cause_of(generated, it->second.first, it->second.second);
    }

    [[nodiscard]] const std::string& note() const noexcept { return note_; }

    /**
     * @brief The wall time evaluating HEAD took, none when it did not run.
     */
    [[nodiscard]] std::optional<double> ms() const noexcept { return ms_; }

private:
    enum class state : std::uint8_t {
        changed, clean, reconfigured, none, compared
    };

    void evaluate(const ct::dependency_record& record) {
        const auto start = std::chrono::steady_clock::now();
        auto at = o_;
        at.commit = head_;
        pipeline p;
        list_files(p, at);
        load_files(p, at);
        run_units(p, at, record);
        format(p, at);
        for (std::uint32_t pass = 1;
             !p.summary.failed() && pass < max_passes; ++pass) {
            auto changes = generated_changes(p, at);
            std::vector<file_change> files;
            for (const auto& c : changes)
                files.push_back(c.file);
            if (fed_back(p.run, files).empty())
                break;
            overlay(p, std::move(changes));
            run_again(p, at, record);
            format(p, at);
        }
        ms_ = since(start);
        const auto& s = p.summary;
        if (s.failed()) {
            state_ = state::none;
            note_ = "HEAD does not load, so the changes cannot be told from "
                "drift: " + (s.errors.empty() ? s.problems : s.errors).front();
            return;
        }
        const auto outputs = codec::ops::outputs_of(p.codecs, p.run);
        const auto& files = p.commit->files;
        for (std::uint32_t row = 0; row < outputs.size(); ++row) {
            const ct::file_id f{row};
            const auto path = outputs.files.paths[f];
            std::optional<store::types::blob_id> committed;
            if (const auto in = checkout_path(at, path)) {
                const auto r = files.find(*in);
                if (r && store::ops::is_regular_file(files.modes[*r]))
                    committed = files.ids[*r];
            }
            files_.insert_or_assign(std::string(path),
                std::pair{outputs.hashes[f], committed});
        }
        state_ = state::compared;
    }

    const types::options& o_;
    bool observed_ = false;
    state state_ = state::changed;
    store::types::object_id head_;
    std::string note_;
    std::optional<double> ms_;
    std::map<std::string, std::pair<store::types::blob_id,
        std::optional<store::types::blob_id>>, std::less<>> files_;
};

/**
 * @brief The input the codec @p codec of @p p says it rendered the file
 * @p content from, relative to the checkout, or empty.
 */
std::string origin_of(const pipeline& p, std::string_view codec,
    std::string_view content) {
    const auto& r = p.codecs;
    for (std::uint32_t c = 0; c < r.size(); ++c) {
        const auto& d = r.codecs[c];
        if (d.name != codec)
            continue;
        if (d.origin == nullptr)
            return {};
        return d.origin({d.options, codec::ops::option_values(r,
            ct::codec_id{c})}, content, p.files.checkout);
    }
    return {};
}

/**
 * @brief The file at @p path, relative to the output root, that
 * @p action changes, as the codec @p codec of @p p produced it from
 * @p content, with the unit @p producers says produced it.
 */
types::attributed_file describe(const pipeline& p,
    const std::unordered_map<std::string_view, ops::producer>& producers,
    std::string_view codec, std::string_view path, std::string_view content,
    codec::io::output_action action) {
    types::attributed_file f;
    f.path = path;
    f.action = action;
    f.codec = codec;
    if (action == codec::io::output_action::remove)
        return f;
    if (const auto it = producers.find(path); it != producers.end())
        f.unit = p.run.record.documents[ct::record_unit_id{it->second.unit}];
    f.origin = origin_of(p, codec, content);
    return f;
}

/**
 * @brief What generate and diff take of each pass's outputs.
 *
 * scope holds the paths of --scope, relative to the checkout, and grows
 * by each file a pass changes, so the next pass also takes what it feeds;
 * given holds them as given. When scope is not empty, only the outputs of
 * the units that read one of its paths, or that their codec says were
 * rendered from one, are taken. Under --skip-stale, an output that would
 * change and was already stale at HEAD is not taken either, and left
 * holds each such file.
 */
struct output_choice {
    head_comparison& head;
    std::vector<std::string> given;
    std::vector<std::string> scope;
    bool skip_stale = false;
    std::map<std::string, types::attributed_file, std::less<>> left;

    [[nodiscard]] bool narrows() const noexcept {
        return !scope.empty() || skip_stale;
    }

    /**
     * @brief Adds @p changes, what a pass changed, to the scope, when
     * there is one.
     */
    void widen(std::span<const file_change> changes) {
        if (scope.empty())
            return;
        for (const auto& c : changes) {
            if (!std::ranges::contains(scope, c.path))
                scope.push_back(c.path);
        }
    }
};

/**
 * @brief The paths of --scope of @p o, relative to its checkout.
 */
std::vector<std::string> scope_of(const types::options& o) {
    std::vector<std::string> out;
    for (const auto& given : o.scope) {
        std::filesystem::path path(given);
        if (path.is_absolute())
            path = path.lexically_normal().lexically_relative(
                o.checkout.lexically_normal());
        out.push_back(path.lexically_normal().generic_string());
    }
    return out;
}

/**
 * @brief Narrows @p outputs, those of the run of @p p, to what @p c
 * takes.
 */
void choose(ct::output_set& outputs, const pipeline& p,
    const types::options& o, output_choice& c) {
    const auto producers = ops::producers_of(p.run.record);
    const auto reading = ops::units_reading(p.run.record, c.scope);
    ops::narrow(outputs, [&](std::size_t row) {
        const ct::file_id f{static_cast<std::uint32_t>(row)};
        const auto path = outputs.files.paths[f];
        const auto codec = outputs.codecs[f];
        const auto content = outputs.files.contents[f];
        if (!c.scope.empty()) {
            const auto it = producers.find(path);
            const bool read = it != producers.end() &&
                reading[it->second.unit] != 0;
            if (!read && !std::ranges::contains(c.scope,
                    origin_of(p, codec, content)))
                return false;
        }
        if (!c.skip_stale)
            return true;
        const auto in = checkout_path(o, path);
        const auto now = in
            ? (ct::holds_file(&p.checkout, *in)
                  ? p.checkout.content_of(*in) : std::nullopt)
            : [&]() -> std::optional<store::types::blob_id> {
                  const auto bytes = read_bytes(o.output_root / path);
                  return bytes ? std::optional(store::ops::blob_id_of(*bytes))
                               : std::nullopt;
              }();
        if (now == outputs.hashes[f] || c.head.cause(path, outputs.hashes[f],
                p.previous.record) != types::change_cause::stale)
            return true;
        auto left = describe(p, producers, codec, path, content,
            now ? codec::io::output_action::write
                : codec::io::output_action::restore);
        left.cause = types::change_cause::stale;
        left.written = false;
        c.left.insert_or_assign(std::string(path), std::move(left));
        return false;
    });
}

/**
 * @brief The filter that narrows the outputs of @p p to what @p c takes,
 * or none when it takes them all.
 */
output_filter filter_of(const pipeline& p, const types::options& o,
    output_choice& c) {
    if (!c.narrows())
        return {};
    return [&p, &o, &c](ct::output_set& outputs) {
        choose(outputs, p, o, c);
    };
}

/**
 * @brief Sorts @p files, the files a command changed and those it left,
 * into @p into, with the note and scope of @p c.
 */
void finish_attribution(types::attribution& into,
    std::map<std::string, types::attributed_file, std::less<>> files,
    output_choice& c) {
    files.merge(c.left);
    for (auto& [path, f] : files)
        into.files.push_back(std::move(f));
    into.note = c.head.note();
    into.scope = c.given;
    into.skip_stale = c.skip_stale;
}

/**
 * @brief Adds to @p files each file @p written, what writing the outputs
 * of @p p did, changed, with what produced it and why it changed.
 */
void attribute_written(const pipeline& p, const types::options& o,
    const sink_outcome& written, output_choice& c,
    std::map<std::string, types::attributed_file, std::less<>>& files) {
    const auto producers = ops::producers_of(p.run.record);
    for (std::size_t k = 0; k < written.actions.size(); ++k) {
        const auto& [path, action] = written.actions[k];
        if (!codec::io::changes_file(action))
            continue;
        std::optional<store::types::blob_id> generated;
        std::string_view content;
        if (action != codec::io::output_action::remove) {
            const auto it = producers.find(path);
            if (it != producers.end())
                content = p.run.record.files.contents[it->second.file];
            if (const auto bytes = read_bytes(o.output_root / path))
                generated = store::ops::blob_id_of(*bytes);
        }
        auto f = describe(p, producers, written.codecs[k], path, content,
            action);
        f.cause = c.head.cause(path, generated, p.previous.record);
        c.left.erase(path);
        files.insert_or_assign(path, std::move(f));
    }
}

types::document_ids document_ids_of(const pipeline& p);

/**
 * @brief The most stale files diff blames; more are left to check, which
 * blames every drift finding.
 */
constexpr std::size_t stale_blamed = 20;

/**
 * @brief Names, by blame's drift method, the commits that likely made
 * each stale file of @p a, files @p plan, the last plan of diff over
 * @p p, changes, unless @p o blames nothing or there are more than
 * stale_blamed of them.
 */
void blame_stale(const pipeline& p, const types::options& o,
    const codec::io::output_plan& plan, types::attribution& a) {
    if (o.blame == types::blame_mode::off)
        return;
    std::map<std::string, std::size_t, std::less<>> stale;
    for (std::size_t i = 0; i < a.files.size(); ++i) {
        const auto& f = a.files[i];
        if (f.cause != types::change_cause::stale || !f.written)
            continue;
        if (auto in = checkout_path(o, f.path))
            stale.emplace(std::move(*in), i);
    }
    if (stale.empty() || stale.size() > stale_blamed)
        return;
    std::vector<types::finding> findings;
    for (auto& f : drift_of(plan, o.output_root, o.checkout)) {
        if (stale.contains(f.path))
            findings.push_back(std::move(f));
    }
    const auto blamed = blame(o, findings, document_ids_of(p), p.run.record);
    for (const auto& b : blamed.findings) {
        if (const auto it = stale.find(b.found.path); it != stale.end())
            a.files[it->second].culprits = b.culprits;
    }
}

/**
 * @brief Each file @p plan, the last plan of diff over @p p, changes,
 * with what produced it and why it changes, and those @p c left.
 */
types::attribution attribution_of(const pipeline& p,
    const codec::io::output_plan& plan, output_choice& c) {
    const auto producers = ops::producers_of(p.run.record);
    std::map<std::string, types::attributed_file, std::less<>> files;
    for (std::size_t i = 0; i < plan.size(); ++i) {
        const auto action = plan.actions[i];
        if (!codec::io::changes_file(action))
            continue;
        auto f = describe(p, producers, plan.codecs[i], plan.paths[i],
            plan.contents[i], action);
        const auto generated = action == codec::io::output_action::remove
            ? std::nullopt
            : std::optional(store::ops::blob_id_of(plan.contents[i]));
        f.cause = c.head.cause(plan.paths[i], generated, p.previous.record);
        if (const auto it = c.left.find(plan.paths[i]); it != c.left.end())
            c.left.erase(it);
        files.emplace(f.path, std::move(f));
    }
    types::attribution out;
    finish_attribution(out, std::move(files), c);
    return out;
}


}

types::generate_result generate(const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    types::generate_result r;
    r.root = o.output_root;
    std::map<std::string, codec::io::output_action> earlier;
    std::map<std::string, std::string> originals;
    head_comparison head(o);
    head.observe();
    output_choice choice{head, o.scope, scope_of(o), o.skip_stale, {}};
    std::map<std::string, types::attributed_file, std::less<>> changed;
    for (r.passes = 1;; ++r.passes) {
        pipeline p;
        evaluate(p, o);
        format(p, o);
        persist(p, o.threads);
        if (p.summary.failed()) {
            add_pass(r.run, std::move(p.summary), r.passes);
            break;
        }
        sink_outcome written;
        p.timed("write", [&] {
            written = write_outputs(o.checkout, o.output_root, p.codecs,
                p.run, filter_of(p, o, choice));
        });
        if (written.sink.problems.empty())
            attribute_written(p, o, written, choice, changed);
        add_pass(r.run, std::move(p.summary), r.passes);
        originals.merge(written.before);
        r.sink = std::move(written.sink);
        if (!r.sink.problems.empty())
            break;
        const auto changes = written_changes(p, o, written.actions);
        choice.widen(changes);
        const auto fed = fed_back(p.run, changes);
        if (fed.empty() || r.passes == max_passes) {
            count_earlier(r.sink, earlier, written.actions);
            if (!fed.empty())
                r.sink.problems.push_back(unsettled(fed));
            break;
        }
        for (const auto& [path, action] : written.actions) {
            if (codec::io::changes_file(action))
                earlier[path] = action;
        }
    }
    r.hand_edits = hand_edits_left(o.output_root, originals);
    finish_attribution(r.changes, std::move(changed), choice);
    if (const auto ms = head.ms())
        r.run.steps.emplace_back("head", *ms);
    r.ms = since(start);
    return r;
}

types::diff_result diff(const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    types::diff_result r;
    r.root = o.output_root;
    pipeline p;
    evaluate(p, o);
    format(p, o);
    head_comparison head(o);
    output_choice choice{head, o.scope, scope_of(o), o.skip_stale, {}};
    for (r.passes = 1; !p.summary.failed(); ++r.passes) {
        planned_outputs planned;
        p.timed("plan", [&] {
            plan_outputs(planned, o.checkout, o.output_root, p.codecs, p.run,
                filter_of(p, o, choice));
        });
        auto changes = planned_changes(p, o, planned.plan);
        std::vector<file_change> files;
        for (const auto& c : changes)
            files.push_back(c.file);
        choice.widen(files);
        const auto fed = fed_back(p.run, files);
        if (!planned.plan.problems.empty() || fed.empty() ||
            r.passes == max_passes) {
            r.planned = codec::io::summarise(planned.plan);
            r.diff = codec::io::diff_outputs(planned.plan);
            if (!fed.empty() && planned.plan.problems.empty())
                r.planned.problems.push_back(unsettled(fed));
            if (planned.plan.problems.empty()) {
                r.changes = attribution_of(p, planned.plan, choice);
                p.timed("blame", [&] {
                    blame_stale(p, o, planned.plan, r.changes);
                });
            }
            break;
        }
        overlay(p, std::move(changes));
        run_again(p, o);
        format(p, o);
    }
    r.run = p.summary;
    if (const auto ms = head.ms())
        r.run.steps.emplace_back("head", *ms);
    r.ms = since(start);
    return r;
}

/**
 * @brief Adds to @p out one finding per diagnostic of the run of @p p,
 * located in the bytes of its document or file.
 */
void add_findings(const pipeline& p, std::vector<types::finding>& out) {
    for (std::size_t c = 0; c < p.codecs.size(); ++c) {
        const auto& d = p.codecs.codecs[c];
        const auto& t = p.run.diagnostics[c];
        for (std::uint32_t i = 0; i < t.size(); ++i) {
            const ct::diagnostic_id id{i};
            const auto document = t.documents[id];
            std::string_view path;
            std::string_view source;
            std::optional<std::string> file;
            if (document.is_none()) {
                path = t.files[id];
                file = p.checkout.read(path);
                if (file)
                    source = *file;
            } else {
                path = p.corpus.paths[document.value];
                source = p.corpus.sources[document.value];
            }
            const auto [line, column] =
                position_of(source, t.ranges[id].begin);
            out.push_back({
                .path = std::string(path),
                .line = line,
                .column = column,
                .rule = std::string(d.name) + "/" +
                    std::string(d.rules[t.rules[id].value].name),
                .level = d.level == ct::severity::warning ? d.level
                                                          : t.levels[id],
                .message = std::string(t.messages[id]),
                .fix = std::string(t.fixes[id]),
                .drift = {},
                .subject = std::string(t.subjects[id]),
                .subject_text = std::string(t.subject_texts[id])});
        }
    }
}

/**
 * @brief Adds to @p out a finding for each file of a formatting check of
 * @p p that its formatter would change, at the first byte that differs.
 * Its formatted form is the file the check holds after the format step;
 * the file itself is read from the checkout, or the commit, of @p p.
 */
void add_format_findings(const pipeline& p, const types::options& o,
    std::vector<types::finding>& out) {
    for (std::uint32_t c = 0; c < p.codecs.size(); ++c) {
        const auto& d = p.codecs.codecs[c];
        if (!d.formatting_check || c >= p.run.formatted_ids.size() ||
            !p.run.formatted_ids[c])
            continue;
        const auto formatter = d.formatters({d.options,
            codec::ops::option_values(p.codecs, ct::codec_id{c})},
            o.checkout).front().name;
        const auto& files = p.run.files[c];
        for (std::uint32_t f = 0; f < files.size(); ++f) {
            const auto path = files.paths[ct::file_id{f}];
            const auto formatted = files.contents[ct::file_id{f}];
            const auto file = p.checkout.read(path);
            if (!file || *file == formatted)
                continue;
            const auto at = static_cast<std::uint32_t>(std::ranges::mismatch(
                *file, formatted).in1 - file->begin());
            const auto [line, column] = position_of(*file, at);
            out.push_back({
                .path = std::string(path),
                .line = line,
                .column = column,
                .rule = std::string(d.name) + "/" +
                    std::string(d.rules.front().name),
                .level = d.level,
                .message = formatter + " formats this file differently",
                .fix = "format it with " + formatter + ", as " +
                    std::string(d.name) + " configures it",
                .drift = {},
                .subject = {},
                .subject_text = {}});
        }
    }
}

/**
 * @brief A file whose exception comments check reads: its path, its
 * content ID when known and, for a document, its bytes.
 */
struct scanned_file {
    std::string_view path;
    std::optional<store::types::blob_id> id;
    std::optional<std::string_view> source;
};

/**
 * @brief Every document of @p p, every other file its codecs read and
 * every file @p findings name, once each.
 */
std::vector<scanned_file> files_to_scan(const pipeline& p,
    const std::vector<types::finding>& findings) {
    std::vector<scanned_file> out;
    for (std::size_t i = 0; i < p.corpus.size(); ++i)
        out.push_back({p.corpus.paths[i], p.corpus.contents[i],
            p.corpus.sources[i]});
    const std::unordered_set<std::string_view> documents(
        p.corpus.paths.begin(), p.corpus.paths.end());
    std::vector<scanned_file> files;
    const auto& record = p.run.record;
    for (std::uint32_t i = 0; i < record.input_count(); ++i) {
        const ct::input_id id{i};
        if (record.input_kinds[id] != ct::input_kind::file)
            continue;
        const auto path = record.input_keys[id];
        if (!documents.contains(path))
            files.push_back({path, record.input_found[id]
                ? std::optional(record.input_values[id]) : std::nullopt, {}});
    }
    for (const auto& f : findings)
        if (!documents.contains(f.path))
            files.push_back({f.path, std::nullopt, {}});
    std::ranges::stable_sort(files, {}, &scanned_file::path);
    const auto [first, last] = std::ranges::unique(files, {},
        &scanned_file::path);
    files.erase(first, last);
    for (auto& f : files)
        if (!f.id)
            f.id = p.checkout.content_of(f.path);
    out.insert(out.end(), files.begin(), files.end());
    return out;
}

/**
 * @brief The exception comments of the files of @p p check reads. A file
 * whose content ID @p unmarked holds is not read, and one that cannot be
 * read holds none. @p unmarked becomes the content IDs of the files that
 * hold no marker.
 */
ct::exception_set exceptions_of(const pipeline& p,
    const std::vector<types::finding>& findings, unsigned threads,
    std::vector<store::types::blob_id>& unmarked) {
    const auto files = files_to_scan(p, findings);
    std::vector<ct::exception_set> all(files.size());
    std::vector<std::uint8_t> clean(files.size(), 0);
    codec::ops::parallel_for(files.size(), threads, [&](std::size_t i) {
        const auto& f = files[i];
        if (f.id && std::ranges::binary_search(unmarked, *f.id)) {
            clean[i] = 1;
            return;
        }
        std::optional<std::string> bytes;
        auto text = f.source;
        if (!text && (bytes = p.checkout.read(f.path)))
            text = *bytes;
        if (!text)
            return;
        if (text->find(codec::ops::exception_marker) == std::string::npos)
            clean[i] = 1;
        else
            all[i] = codec::ops::parse_exceptions(f.path, *text);
    });
    unmarked.clear();
    for (std::size_t i = 0; i < files.size(); ++i)
        if (clean[i] && files[i].id)
            unmarked.push_back(*files[i].id);
    std::ranges::sort(unmarked);
    unmarked.erase(std::unique(unmarked.begin(), unmarked.end()),
        unmarked.end());
    ct::exception_set out;
    for (auto& f : all)
        codec::ops::append(out, std::move(f));
    return out;
}

/**
 * @brief Drops each finding of @p r that a valid exception of the files
 * of @p p covers as of @p o's today, counting it in excused, and adds the
 * findings about the exceptions themselves. The record of the contents
 * that hold no marker is read, and written for a check of the checkout.
 */
void excuse_findings(const pipeline& p, const types::options& o,
    types::check_result& r) {
    const auto record = codec::io::unmarked_file_path(
        store::io::git_dir(o.checkout));
    auto unmarked = codec::io::read_unmarked_file(record);
    auto set = exceptions_of(p, r.findings, o.threads, unmarked);
    if (!p.commit)
        codec::io::write_unmarked_file(record, unmarked);
    codec::ops::append(set, ct::exception_set(o.exceptions));

    std::vector<codec::ops::finding_site> sites;
    sites.reserve(r.findings.size());
    for (const auto& f : r.findings)
        sites.push_back({f.path, f.line, f.rule});
    auto e = codec::ops::excuse(sites, set, p.codecs, o.today);
    std::vector<types::finding> kept;
    for (std::size_t i = 0; i < r.findings.size(); ++i) {
        if (e.excused[i])
            ++r.excused;
        else
            kept.push_back(std::move(r.findings[i]));
    }
    for (auto& f : e.findings)
        kept.push_back({.path = std::move(f.path),
            .line = f.line,
            .column = 1,
            .rule = std::move(f.rule),
            .level = codec::types::severity::error,
            .message = std::move(f.message),
            .fix = std::move(f.fix),
            .drift = {},
            .subject = {},
            .subject_text = {}});
    r.findings = std::move(kept);
}

namespace {

/**
 * @brief Checks the checkout of @p o, or the commit it names, into @p p,
 * which keeps the corpus the check read.
 */
types::check_result check_in(pipeline& p, const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    types::check_result r;
    evaluate(p, o);
    format(p, o);
    persist(p, o.threads);
    if (!p.summary.failed() && p.commit) {
        p.timed("plan", [&] {
            const auto outputs = codec::ops::outputs_of(p.codecs, p.run);
            const auto problems = codec::ops::check_outputs(outputs);
            for (const auto& problem : problems)
                p.summary.errors.push_back("output: " + problem);
            if (!problems.empty())
                return;
            r.findings = commit_drift_of(*p.commit, outputs,
                in_commit(o.output_root, o.checkout));
            std::ranges::move(names_missing_of(outputs, p.listed.files),
                std::back_inserter(r.findings));
        });
    } else if (!p.summary.failed()) {
        p.timed("plan", [&] {
            planned_outputs planned;
            plan_outputs(planned, o.checkout, o.output_root, p.codecs, p.run);
            for (const auto& problem : planned.plan.problems)
                p.summary.errors.push_back("output: " + problem);
            r.findings = drift_of(planned.plan, o.output_root, o.checkout);
            if (planned.plan.problems.empty())
                std::ranges::move(names_missing_of(planned.outputs,
                    p.listed.files), std::back_inserter(r.findings));
        });
    }
    add_findings(p, r.findings);
    add_format_findings(p, o, r.findings);
    if (!p.summary.failed())
        p.timed("exceptions", [&] {
            excuse_findings(p, o, r);
        });
    r.run = p.summary;
    std::ranges::stable_sort(r.findings, {}, [](const types::finding& f) {
        return std::tie(f.path, f.line, f.column);
    });
    r.ms = since(start);
    return r;
}

/**
 * @brief The documents of the corpus of @p p that have an :ID:, by it,
 * with their path and #+type:.
 */
types::document_ids document_ids_of(const pipeline& p) {
    types::document_ids out;
    const auto& c = p.corpus;
    for (std::uint32_t row = 0; row < c.size(); ++row) {
        const auto& tables = c.tables[row];
        const auto& source = c.sources[row];
        const auto id = org::ops::find_document_property(tables, "ID");
        if (id.is_none())
            continue;
        const auto key = org::ops::trimmed(source,
            tables.properties.values[id]).of(source);
        const auto keyword = [&](std::string_view name) {
            const auto k = org::ops::find_keyword(tables, name);
            return k.is_none() ? std::string() : std::string(
                org::ops::trimmed(source, tables.keywords.values[k])
                    .of(source));
        };
        out.try_emplace(std::string(key), types::document_ref{
            std::string(c.paths[row]), keyword("type"), keyword("title")});
    }
    return out;
}

/**
 * @brief Checks the commits from @p from to @p to, and the pull request
 * of the branch, as the commit_provenance codec of @p p is configured,
 * against the documents of @p p; nothing is checked when the codec is
 * off.
 */
types::provenance_result provenance_of(const pipeline& p,
    const types::options& o, const store::types::object_id& from,
    const store::types::object_id& to) {
    const auto& codecs = p.codecs.codecs;
    const auto it = std::ranges::find(codecs, ops::provenance_codec,
        &ct::descriptor::name);
    if (it == codecs.end())
        return {};
    const ct::codec_id id{static_cast<std::uint32_t>(it - codecs.begin())};
    if (p.codecs.enabled[id.value] == 0)
        return {};
    const ct::option_values values{it->options,
        codec::ops::option_values(p.codecs, id)};
    const auto rules = ops::provenance_rules_of(
        values[codec::ops::data_option], values["require_pull_request"]);
    if (!rules) {
        types::provenance_result r;
        r.note = "not checked: " + rules.error();
        return r;
    }
    const auto commits = io::branch_commits(o.checkout, from, to);
    const auto branch = io::head_branch(o.checkout);
    const auto pr = io::find_pull_request(o.checkout, values["gh"], branch);
    return ops::check_provenance(*rules, commits, document_ids_of(p),
        pr ? std::optional(*pr) : std::nullopt, pr ? "" : pr.error(),
        branch);
}

}

types::check_result check(const types::options& o) {
    pipeline p;
    auto r = check_in(p, o);
    if (o.blame != types::blame_mode::off && !o.commit && !r.run.failed())
        r.blame = blame(o, r.findings, document_ids_of(p), p.run.record);
    return r;
}

types::check_result blame_one(const types::options& o,
    const ops::finding_target& target) {
    auto only = o;
    only.blame = types::blame_mode::bisect;
    const auto codec = target.rule.substr(0, target.rule.find('/'));
    for (const auto& d : codecs())
        only.settings.set(d.name, ct::enabled_key,
            d.name == codec ? "true" : "false");
    if (o.checks)
        for (const auto& k : o.checks->checks)
            only.settings.set(k->codec.name, ct::enabled_key,
                k->codec.name == codec ? "true" : "false");
    pipeline p;
    auto r = check_in(p, only);
    const types::finding* best = nullptr;
    for (const auto& f : r.findings) {
        if (f.path != target.path || f.rule != target.rule)
            continue;
        const auto distance = [&](const types::finding& x) {
            return x.line > target.line ? x.line - target.line
                                        : target.line - x.line;
        };
        if (best == nullptr || distance(f) < distance(*best))
            best = &f;
    }
    if (best == nullptr) {
        r.findings.clear();
        return r;
    }
    const auto found = *best;
    r.findings = {found};
    if (!r.run.failed())
        r.blame = blame(only, r.findings, document_ids_of(p), p.run.record);
    return r;
}

namespace {

std::string joined(std::string_view a, std::string_view b,
    std::string_view c = {}) {
    std::string out(a);
    out.push_back('\0');
    out += b;
    out.push_back('\0');
    out += c;
    return out;
}

/**
 * @brief How many findings of the units of @p record name each path, rule
 * and message, read with the corpus and codecs of @p p.
 */
std::unordered_map<std::string, std::size_t> recorded_findings(
    const pipeline& p, const ct::dependency_record& record) {
    std::unordered_map<std::string, std::size_t> out;
    const auto index = codec::ops::index_corpus(p.corpus);
    for (std::uint32_t u = 0; u < record.size(); ++u) {
        const ct::record_unit_id unit{u};
        const auto codec = std::ranges::find(p.codecs.codecs,
            record.codecs[unit], &ct::descriptor::name);
        ct::diagnostic_table t;
        if (codec == p.codecs.codecs.end() ||
            !codec::ops::decode_diagnostics(record.diagnostics[unit], index,
                &t))
            continue;
        for (std::uint32_t i = 0; i < t.size(); ++i) {
            const ct::diagnostic_id id{i};
            const auto rule = t.rules[id].value;
            if (rule >= codec->rules.size())
                continue;
            const auto document = t.documents[id];
            const auto path = document.is_none() ? t.files[id]
                : p.corpus.paths[document.value];
            ++out[joined(path, std::string(codec->name) + "/" +
                std::string(codec->rules[rule].name), t.messages[id])];
        }
    }
    return out;
}

/**
 * @brief The ID property of the document at @p path in the corpus of
 * @p p, or none.
 */
std::vector<std::string_view> ids_of(const pipeline& p,
    std::string_view path) {
    const auto& c = p.corpus;
    const auto it = std::ranges::find(c.paths, path);
    if (it == c.paths.end())
        return {};
    const auto row = static_cast<std::size_t>(it - c.paths.begin());
    const auto& tables = c.tables[row];
    const auto property = org::ops::find_document_property(tables, "ID");
    if (property.is_none())
        return {};
    const auto source = c.sources[row];
    const auto id = org::ops::trimmed(source,
        tables.properties.values[property]).of(source);
    if (id.empty())
        return {};
    return {id};
}

/**
 * @brief Whether codec @p c of @p r may read any document.
 */
bool reads_documents(const ct::registry& r, std::size_t c) {
    const auto flags = std::span(r.reads).subspan(c * r.type_slots,
        r.type_slots);
    return std::ranges::any_of(flags, [](std::uint8_t f) { return f != 0; });
}

/**
 * @brief The heads of some units of a dependency record, with why they
 * could not be read in loaded's problem, and each unit's codec and
 * document joined.
 */
struct unit_heads {
    codec::io::loaded_record loaded;
    std::unordered_set<std::string> names;
};

unit_heads read_heads(const std::filesystem::path& path,
    const codec::io::loaded_index& index,
    std::span<const ct::record_unit_id> units) {
    unit_heads r;
    r.loaded = codec::io::read_record_units(path, index.index, units);
    const auto& record = r.loaded.record;
    for (std::uint32_t u = 0; u < record.size(); ++u)
        r.names.insert(joined(record.codecs[{u}], record.documents[{u}]));
    return r;
}

/**
 * @brief The drift of the outputs of the run of @p p an edit to the file
 * at @p path may have changed: each output whose blob ID differs from the
 * one @p previous recorded for it or that is that file, and each output
 * @p previous recorded that the run no longer generates. @p units are the
 * units of the record file at @p record that @p previous holds the heads
 * of, in order. Only those outputs are formatted and compared with the
 * disk, and only the contents of the edited file are read from the
 * record, when a reused unit produced it.
 */
std::vector<types::finding> hook_drift(pipeline& p, const types::options& o,
    const std::filesystem::path& record, const codec::io::loaded_index& index,
    std::span<const ct::record_unit_id> units,
    const ct::dependency_record& previous, std::string_view path) {
    struct recorded_file {
        store::types::blob_id hash;
        ct::record_unit_id unit;
        std::uint32_t file = 0;
    };
    std::unordered_map<std::string, recorded_file> recorded;
    for (std::uint32_t u = 0; u < previous.size(); ++u) {
        const ct::record_unit_id unit{u};
        const auto first = previous.file_firsts[unit].value;
        const auto end = u + 1 < previous.size()
            ? previous.file_firsts[unit.next()].value
            : static_cast<std::uint32_t>(previous.files.size());
        for (auto f = first; f < end; ++f)
            recorded.insert_or_assign(joined(previous.codecs[unit],
                previous.files.paths[ct::file_id{f}]),
                recorded_file{previous.file_hashes[ct::file_id{f}],
                    units[u], f - first});
    }

    const auto in_checkout = [&](std::string_view output) {
        return (o.output_root / output).lexically_relative(o.checkout)
            .generic_string();
    };
    ct::run_result changed;
    changed.files.resize(p.codecs.size());
    std::unordered_set<std::string> generated;
    const auto& run = p.run.record;
    const auto files_end = [&](std::uint32_t u) {
        return u + 1 < run.size()
            ? run.file_firsts[ct::record_unit_id{u + 1}].value
            : static_cast<std::uint32_t>(run.files.size());
    };
    // A path two parts of a staged codec emit is the later part's, in the
    // record above and in the run alike.
    std::unordered_map<std::string, std::uint32_t> last;
    for (std::uint32_t u = 0; u < run.size(); ++u) {
        const ct::record_unit_id unit{u};
        for (auto f = run.file_firsts[unit].value; f < files_end(u); ++f)
            last.insert_or_assign(joined(run.codecs[unit],
                run.files.paths[ct::file_id{f}]), f);
    }
    for (std::uint32_t u = 0; u < run.size(); ++u) {
        const ct::record_unit_id unit{u};
        const auto codec = std::ranges::find(p.codecs.codecs, run.codecs[unit],
            &ct::descriptor::name) - p.codecs.codecs.begin();
        const bool staged = ct::unit_of(p.codecs.codecs[
            static_cast<std::size_t>(codec)]) == ct::unit::staged;
        for (auto f = run.file_firsts[unit].value; f < files_end(u); ++f) {
            const ct::file_id id{f};
            const auto file = run.files.paths[id];
            auto key = joined(run.codecs[unit], file);
            if (staged && last.find(key)->second != f)
                continue;
            const auto it = recorded.find(key);
            generated.insert(key);
            const bool same = it != recorded.end() &&
                it->second.hash == run.file_hashes[id];
            if (same && in_checkout(file) != path)
                continue;
            std::optional<std::string> bytes;
            auto content = run.files.contents[id];
            if (same && content.empty() && run.file_hashes[id] !=
                    store::ops::blob_id_of("")) {
                bytes = codec::io::read_recorded_file(record, index.index,
                    it->second.unit, it->second.file);
                if (!bytes)
                    throw std::runtime_error("the record lost the contents "
                        "of " + std::string(file));
                content = *bytes;
            }
            changed.files[static_cast<std::size_t>(codec)].add(file, content);
        }
    }
    p.summary.formats = codec::io::format_outputs(p.codecs, changed,
        o.checkout, store::io::git_common_dir(o.checkout), o.threads);

    ct::output_set outputs;
    std::unordered_set<std::string> planned;
    for (std::size_t c = 0; c < p.codecs.size(); ++c) {
        const auto& files = changed.files[c];
        const auto& ids = changed.formatted_ids[c];
        for (std::uint32_t f = 0; f < files.size(); ++f) {
            const ct::file_id id{f};
            const auto content = files.contents[id];
            outputs.add(p.codecs.codecs[c].name, files.paths[id], content,
                ids ? (*ids)[f] : store::ops::blob_id_of(content));
            planned.emplace(files.paths[id]);
        }
    }
    for (const auto& entry : recorded) {
        const auto& key = entry.first;
        if (generated.contains(key))
            continue;
        const auto codec = key.substr(0, key.find('\0'));
        const auto file = key.substr(codec.size() + 1);
        planned.emplace(file.substr(0, file.size() - 1));
        const auto c = static_cast<std::size_t>(std::ranges::find(
            p.codecs.codecs, codec, &ct::descriptor::name) -
            p.codecs.codecs.begin());
        const bool refused = c < p.codecs.size() &&
            codec::ops::refused(p.codecs, p.run, c);
        if (!refused && !std::ranges::contains(outputs.complete, codec))
            outputs.complete.emplace_back(codec);
    }

    planned_outputs plan;
    plan.previous = codec::io::read_written_outputs(
        codec::io::written_outputs_path(store::io::git_dir(o.checkout)));
    auto& written = plan.previous.record;
    ct::written_outputs kept;
    kept.root = written.root;
    kept.written = written.written;
    for (std::size_t i = 0; i < written.size(); ++i) {
        if (!planned.contains(written.paths[i]))
            continue;
        kept.paths.push_back(written.paths[i]);
        kept.codecs.push_back(written.codecs[i]);
        kept.hashes.push_back(written.hashes[i]);
        kept.stats.push_back(written.stats[i]);
    }
    written = std::move(kept);
    plan.outputs = std::move(outputs);
    plan.plan = codec::io::plan_outputs(o.output_root, plan.outputs, written);
    for (const auto& problem : plan.plan.problems)
        p.summary.errors.push_back("output: " + problem);
    auto found = drift_of(plan.plan, o.output_root, o.checkout);
    if (plan.plan.problems.empty())
        std::ranges::move(names_missing_of(plan.outputs, p.listed.files),
            std::back_inserter(found));
    return found;
}

/**
 * @brief Drops each of @p findings a valid exception covers as of @p o's
 * today, counting it in @p excused, and adds the findings about the
 * exceptions of the file at @p path. Only that file and the files of
 * @p findings are read.
 */
void excuse_hook_findings(const pipeline& p, const types::options& o,
    std::string_view path, std::vector<types::finding>& findings,
    std::size_t& excused) {
    std::vector<std::string_view> paths{path};
    for (const auto& f : findings)
        paths.push_back(f.path);
    std::ranges::sort(paths);
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

    ct::exception_set set = o.exceptions;
    for (const auto file : paths) {
        std::optional<std::string> bytes;
        std::optional<std::string_view> text;
        if (const auto it = std::ranges::find(p.corpus.paths, file);
            it != p.corpus.paths.end())
            text = p.corpus.sources[static_cast<std::size_t>(
                it - p.corpus.paths.begin())];
        else if ((bytes = p.checkout.read(file)))
            text = *bytes;
        if (!text ||
            text->find(codec::ops::exception_marker) == std::string::npos)
            continue;
        codec::ops::append(set, codec::ops::parse_exceptions(file, *text));
    }

    std::vector<codec::ops::finding_site> sites;
    sites.reserve(findings.size());
    for (const auto& f : findings)
        sites.push_back({f.path, f.line, f.rule});
    auto e = codec::ops::excuse(sites, set, p.codecs, o.today);
    std::vector<types::finding> kept;
    for (std::size_t i = 0; i < findings.size(); ++i) {
        if (e.excused[i])
            ++excused;
        else
            kept.push_back(std::move(findings[i]));
    }
    for (auto& f : e.findings) {
        if (f.path != path)
            continue;
        kept.push_back({.path = std::move(f.path),
            .line = f.line,
            .column = 1,
            .rule = std::move(f.rule),
            .level = codec::types::severity::error,
            .message = std::move(f.message),
            .fix = std::move(f.fix),
            .drift = {},
            .subject = {},
            .subject_text = {}});
    }
    findings = std::move(kept);
}

bool is_contract_page(const std::filesystem::path& file,
    const types::options& o) {
    const auto name = file.filename().string();
    return (o.checkout / file).parent_path().lexically_normal() ==
            o.meta.lexically_normal() &&
        name.starts_with("document_type_") && name.ends_with(".org");
}

}

types::hook_result hook(const types::options& o, const std::string& path,
    bool created) {
    const auto start = std::chrono::steady_clock::now();
    types::hook_result r;
    r.path = path;
    pipeline p;
    const auto done = [&] {
        r.run = std::move(p.summary);
        r.ms = since(start);
        return std::move(r);
    };
    if (path == config::repository_file_name ||
        is_contract_page(path, o)) {
        r.note = path + " changes what every codec sees; run compass check";
        return done();
    }

    const auto record = codec::io::dependency_file_path(
        store::io::git_dir(o.checkout));
    codec::io::loaded_index index;
    p.timed("index", [&] { index = codec::io::read_unit_index(record); });
    if (index.missing) {
        if (std::filesystem::exists(o.checkout /
                config::repository_file_name))
            r.note = "no dependency record yet; run compass check once";
        return done();
    }
    if (!index.problem.empty()) {
        r.note = "the dependency record cannot be used (" + index.problem +
            "); run compass check";
        return done();
    }

    const auto relative_out = (o.checkout / path).lexically_relative(
        o.output_root).generic_string();
    codec::ops::edited_file edited{.path = path, .output = {},
        .document = path.ends_with(".org"), .created = created, .ids = {}};
    if (!relative_out.empty() && !relative_out.starts_with(".."))
        edited.output = relative_out;
    auto touched = codec::ops::units_touching(index.index, edited);
    if (touched.empty() && !edited.document)
        return done();

    list_files(p, o, true, created ? std::string_view(path)
        : std::string_view{});
    if (!created && !std::ranges::binary_search(p.listed.files, path)) {
        // A file an unchecked edit created is missing from the record's
        // listing, so the checkout is listed again.
        p.listed.files.clear();
        p.listed.documents.clear();
        p.timed("list", [&] { list(p.listed, o.checkout); });
    }
    if (!std::ranges::binary_search(p.listed.files, path))
        return done();
    unit_heads heads;
    const auto read = [&] {
        p.timed("record", [&] { heads = read_heads(record, index, touched); });
        if (!heads.loaded.problem.empty())
            r.note = "the dependency record changed while it was read (" +
                heads.loaded.problem + "); run compass check";
        return heads.loaded.problem.empty();
    };
    if (!read())
        return done();
    auto with_documents = edited.document;
    const auto& units = heads.loaded.record;
    for (std::uint32_t u = 0; u < units.size() && !with_documents; ++u) {
        const auto codec = std::ranges::find(p.codecs.codecs,
            units.codecs[{u}], &ct::descriptor::name);
        with_documents = codec != p.codecs.codecs.end() &&
            reads_documents(p.codecs,
                static_cast<std::size_t>(codec - p.codecs.codecs.begin()));
    }
    load_files(p, o, with_documents);
    if (p.summary.failed()) {
        r.note = "the checkout did not load (" +
            (p.summary.errors.empty() ? p.summary.problems.front()
                                      : p.summary.errors.front()) +
            "); run compass check";
        return done();
    }
    const auto ids = ids_of(p, path);
    if (edited.document && !ids.empty()) {
        edited.ids = ids;
        auto more = codec::ops::units_touching(index.index, edited);
        if (more != touched) {
            touched = std::move(more);
            if (!read())
                return done();
        }
    }

    run_units(p, o, heads.loaded.record, [&](ct::codec_id c,
        std::string_view document) {
        const auto& codec = p.codecs.codecs[c.value];
        // A staged codec whose prepare step is taken may list a part the
        // record lacks, which is taken too.
        return (edited.document && document == path) ||
            heads.names.contains(joined(codec.name, document)) ||
            (ct::unit_of(codec) == ct::unit::staged &&
                heads.names.contains(joined(codec.name, {})));
    }, path);
    r.checked = true;

    std::vector<types::finding> found;
    add_findings(p, found);
    auto old = recorded_findings(p, heads.loaded.record);
    for (auto& f : found) {
        if (f.path != path || !o.all_findings) {
            const auto it = old.find(joined(f.path, f.rule, f.message));
            if (it != old.end() && it->second != 0) {
                --it->second;
                r.old += f.path == path ? 1 : 0;
                continue;
            }
        }
        r.findings.push_back(std::move(f));
    }
    p.timed("plan", [&] {
        for (auto& f : hook_drift(p, o, record, index, touched,
                 heads.loaded.record, path))
            r.findings.push_back(std::move(f));
    });
    p.timed("exceptions", [&] {
        excuse_hook_findings(p, o, path, r.findings, r.excused);
    });
    std::ranges::stable_sort(r.findings, {}, [](const types::finding& f) {
        return std::tie(f.path, f.line, f.column);
    });
    return done();
}

types::ratchet_result ratchet(const types::options& o,
    std::string_view base) {
    const auto start = std::chrono::steady_clock::now();
    types::ratchet_result r;
    const auto head = io::resolve_commit(o.checkout, "HEAD");
    const auto tip = io::resolve_base(o.checkout, base);
    r.base = tip.name;
    r.merge_base = io::merge_base(o.checkout, head, tip.id);
    pipeline p;
    r.current = check_in(p, o);
    if (!r.current.run.failed())
        r.provenance = provenance_of(p, o, r.merge_base, head);
    auto at_base = o;
    at_base.commit = r.merge_base;
    // A codec HEAD's configuration enables may need files the merge base
    // lacks, such as a library HEAD adds. The base is checked without it,
    // and with nothing at the base to excuse them its findings are new.
    while (true) {
        try {
            r.previous = check(at_base);
            break;
        } catch (const codec::types::codec_failure& e) {
            if (std::ranges::find(r.not_run_at_base, e.codec(),
                    &types::codec_not_run::codec) != r.not_run_at_base.end())
                throw;
            r.not_run_at_base.push_back({e.codec(), e.what()});
            at_base.settings.set(e.codec(), ct::enabled_key, "false");
        }
    }
    r.renames = renames_of(o.checkout, r.merge_base, head,
        r.previous.findings);
    r.split = ops::split_new(r.previous.findings, r.current.findings,
        r.renames);
    std::ranges::copy(r.provenance.findings,
        std::back_inserter(r.split.fresh));
    if (o.blame != types::blame_mode::off && !r.failed())
        r.blame = blame(o, r.split.inherited, document_ids_of(p),
            p.run.record);
    r.ms = since(start);
    return r;
}

types::gc_result gc(const types::options& o) {
    types::gc_result r;
    r.packs = store::io::packs_dir(store::io::git_common_dir(o.checkout));
    r.options = o.collection;
    r.collection = collect(o.checkout, o.collection);
    return r;
}

}

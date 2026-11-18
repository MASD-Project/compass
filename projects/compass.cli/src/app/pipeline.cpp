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
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <ranges>
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
#include "compass.cli/io/listing.hpp"
#include "compass.cli/ops/ratchet.hpp"
#include "compass.codec/io/dependency_file.hpp"
#include "compass.codec/io/format_cache.hpp"
#include "compass.codec/io/unmarked_file.hpp"
#include "compass.codec/ops/dependencies.hpp"
#include "compass.codec/ops/exceptions.hpp"
#include "compass.codec/ops/outputs.hpp"
#include "compass.codec/ops/parallel_for.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.store/io/git_objects.hpp"
#include "compass.store/io/location.hpp"
#include "compass.store/ops/blob_hash.hpp"

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
 * never moved once evaluated.
 */
struct pipeline {
    std::optional<commit_source> commit;
    org::types::type_registry types;
    listing listed;
    corpus_files files;
    ct::checkout_files checkout;
    ct::corpus corpus;
    ct::registry codecs;
    ct::run_result run;
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
    codec::io::loaded_record previous;
    p.timed("record", [&] { previous = read_dependency_record(o.checkout); });
    run_units(p, o, previous.record);
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
 * checkout its worktree index and dependency record. A run over a commit
 * leaves the records of the checkout as they are, since they describe its
 * files.
 */
void persist(pipeline& p, unsigned threads) {
    p.timed("persist", [&] {
        store_objects(p.files);
        store_results(p.files.checkout, p.run, threads);
        if (p.commit)
            return;
        write_worktree_index(p.files);
        write_dependency_record(p.files.checkout, p.run);
        io::write_listing_file(io::listing_file_path(
            store::io::git_dir(p.files.checkout)), p.listed.files,
            p.listed.documents);
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
        .drift = std::move(sides)};
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

}

types::generate_result generate(const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    types::generate_result r;
    r.root = o.output_root;
    pipeline p;
    evaluate(p, o);
    format(p, o);
    persist(p, o.threads);
    if (!p.summary.failed()) {
        p.timed("write", [&] {
            auto written = write_outputs(o.checkout, o.output_root, p.codecs,
                p.run);
            r.sink = std::move(written.sink);
            r.hand_edits = std::move(written.hand_edits);
        });
    }
    r.run = p.summary;
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
    if (!p.summary.failed()) {
        p.timed("plan", [&] {
            planned_outputs planned;
            plan_outputs(planned, o.checkout, o.output_root, p.codecs, p.run);
            r.planned = codec::io::summarise(planned.plan);
            r.diff = codec::io::diff_outputs(planned.plan);
        });
    }
    r.run = p.summary;
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
                .level = t.levels[id],
                .message = std::string(t.messages[id]),
                .fix = std::string(t.fixes[id]),
                .drift = {}});
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
            .drift = {}});
    r.findings = std::move(kept);
}

types::check_result check(const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    types::check_result r;
    pipeline p;
    evaluate(p, o);
    format(p, o);
    persist(p, o.threads);
    if (!p.summary.failed() && p.commit) {
        p.timed("plan", [&] {
            const auto outputs = codec::ops::outputs_of(p.codecs, p.run);
            const auto problems = codec::ops::check_outputs(outputs);
            for (const auto& problem : problems)
                p.summary.errors.push_back("output: " + problem);
            if (problems.empty())
                r.findings = commit_drift_of(*p.commit, outputs,
                    in_commit(o.output_root, o.checkout));
        });
    } else if (!p.summary.failed()) {
        p.timed("plan", [&] {
            planned_outputs planned;
            plan_outputs(planned, o.checkout, o.output_root, p.codecs, p.run);
            for (const auto& problem : planned.plan.problems)
                p.summary.errors.push_back("output: " + problem);
            r.findings = drift_of(planned.plan, o.output_root, o.checkout);
        });
    }
    add_findings(p, r.findings);
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
    return drift_of(plan.plan, o.output_root, o.checkout);
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
            .drift = {}});
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
    r.current = check(o);
    auto at_base = o;
    at_base.commit = r.merge_base;
    r.previous = check(at_base);
    r.renames = renames_of(o.checkout, r.merge_base, head,
        r.previous.findings);
    r.split = ops::split_new(r.previous.findings, r.current.findings,
        r.renames);
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

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
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <vector>
#include "compass.cli/app/steps.hpp"
#include "compass.cli/app/codecs.hpp"
#include "compass.cli/io/listing.hpp"
#include "compass.cli/ops/ratchet.hpp"
#include "compass.codec/io/format_cache.hpp"
#include "compass.codec/io/unmarked_file.hpp"
#include "compass.codec/ops/exceptions.hpp"
#include "compass.codec/ops/outputs.hpp"
#include "compass.codec/ops/parallel_for.hpp"
#include "compass.codec/ops/registry.hpp"
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
        throw std::runtime_error(path.string() + " is outside the checkout, "
            "so a commit cannot hold it");
    return relative == "." ? std::filesystem::path{} : relative;
}

/**
 * @brief Lists, loads and runs the codecs over the checkout of @p o, or
 * the commit it names, into @p p. Reads the checkout or git's objects,
 * the store, the dependency record and the result cache, and writes
 * nothing.
 */
void evaluate(pipeline& p, const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    auto& s = p.summary;
    const auto common = store::io::git_common_dir(o.checkout);
    if (o.commit)
        p.timed("tree", [&] { p.commit.emplace(common, *o.commit); });
    const commit_source* commit = p.commit ? &*p.commit : nullptr;

    org::types::type_registry type_registry;
    p.timed("types", [&] {
        type_registry = commit
            ? build_type_registry(*commit, in_commit(o.meta, o.checkout))
            : build_type_registry(o.meta);
    });

    p.timed("list", [&] {
        if (commit)
            list(p.listed, *commit);
        else
            list(p.listed, o.checkout);
    });
    const auto& documents = p.listed.documents;
    p.files = open_files(o.checkout, store::io::objects_dir(common),
        documents, commit);
    s.files = documents.size();
    p.timed("resolve", [&] { resolve(p.files, o.threads); });
    p.timed("load", [&] { load(p.files, o.threads); });

    for (std::size_t row = 0; row < documents.size(); ++row) {
        if (!p.files.errors[row].empty())
            s.errors.push_back(std::string(documents[row]) + ": " +
                p.files.errors[row]);
        else
            ++(p.files.mapped[row] ? s.loaded : s.parsed);
    }
    p.checkout = checkout_files_of(p.files, p.listed);
    p.corpus = corpus_of(p.files, type_registry);
    p.corpus.files = &p.checkout;
    s.documents = p.corpus.size();

    p.codecs = codec::ops::make_registry(codecs(),
        type_registry, o.settings);
    s.problems = p.codecs.problems;
    codec::io::loaded_record previous;
    p.timed("record", [&] { previous = read_dependency_record(o.checkout); });
    p.timed("codecs", [&] {
        p.run = run_codecs(p.codecs, p.corpus, type_registry, o.threads,
            previous.record, o.checkout);
    });
    for (std::size_t c = 0; c < p.codecs.size(); ++c)
        s.codecs.emplace_back(p.codecs.codecs[c].name);
    s.ran = p.run.ran;
    s.skipped = p.run.skipped;
    s.cached = p.run.cached;
    s.formats.resize(p.codecs.size());
    s.ms = since(start);
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
        .rule = codec + "/drift",
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
                *relative.begin() == ".." ? file.string() : relative.string(),
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
            r.sink = write_outputs(o.checkout, o.output_root, p.codecs,
                p.run).sink;
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
 * @brief The exception comments of the files of @p p check reads, for
 * the files that hold any. A file whose content ID @p unmarked holds is
 * not read, and one that cannot be read holds none. @p unmarked becomes
 * the content IDs of the files that hold no marker.
 */
std::vector<ct::file_exceptions> exceptions_of(const pipeline& p,
    const std::vector<types::finding>& findings, unsigned threads,
    std::vector<store::types::blob_id>& unmarked) {
    const auto files = files_to_scan(p, findings);
    std::vector<ct::file_exceptions> all(files.size());
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
    std::erase_if(all, [](const ct::file_exceptions& f) {
        return f.exceptions.empty() && f.malformed.empty();
    });
    return all;
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
    const auto files = exceptions_of(p, r.findings, o.threads, unmarked);
    if (!p.commit)
        codec::io::write_unmarked_file(record, unmarked);

    std::vector<codec::ops::finding_site> sites;
    sites.reserve(r.findings.size());
    for (const auto& f : r.findings)
        sites.push_back({f.path, f.line, f.rule});
    auto e = codec::ops::excuse(sites, files, p.codecs, o.today);
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
            r.findings.push_back({
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
    r.objects = store::io::objects_dir(store::io::git_common_dir(o.checkout));
    r.options = o.collection;
    r.collection = collect(o.checkout, r.objects, o.collection);
    return r;
}

}

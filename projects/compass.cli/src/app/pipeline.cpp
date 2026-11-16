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
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include "compass.cli/app/steps.hpp"
#include "compass.cli/app/codecs.hpp"
#include "compass.codec/io/format_cache.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.store/io/location.hpp"

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
 * point into listing, so the pipeline is never moved once evaluated.
 */
struct pipeline {
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
 * @brief Lists, loads and runs the codecs over the checkout of @p o into
 * @p p. Reads the checkout, the store, the dependency record and the
 * result cache, and writes nothing.
 */
void evaluate(pipeline& p, const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    auto& s = p.summary;
    org::types::type_registry type_registry;
    p.timed("types", [&] { type_registry = build_type_registry(o.meta); });

    p.timed("list", [&] { list(p.listed, o.checkout); });
    const auto& documents = p.listed.documents;
    p.files = open_files(o.checkout,
        store::io::objects_dir(store::io::git_common_dir(o.checkout)),
        documents);
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
 * parsed, the worktree index, the result cache and the dependency record.
 */
void persist(pipeline& p, unsigned threads) {
    p.timed("persist", [&] {
        store_objects(p.files);
        write_worktree_index(p.files);
        store_results(p.files.checkout, p.run, threads);
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
        const auto [line, column] = position_of(disk.value_or(""), at);
        const auto relative = file.lexically_relative(checkout);
        out.push_back({
            .path = relative.empty() || *relative.begin() == ".."
                ? file.string() : relative.string(),
            .line = line,
            .column = column,
            .rule = codec + "/drift",
            .level = codec::types::severity::error,
            .message = std::move(message),
            .fix = "run compass generate, or change the model or template "
                   "that generates it"});
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

types::check_result check(const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    types::check_result r;
    pipeline p;
    evaluate(p, o);
    format(p, o);
    persist(p, o.threads);
    if (!p.summary.failed()) {
        p.timed("plan", [&] {
            planned_outputs planned;
            plan_outputs(planned, o.checkout, o.output_root, p.codecs, p.run);
            for (const auto& problem : planned.plan.problems)
                p.summary.errors.push_back("output: " + problem);
            r.findings = drift_of(planned.plan, o.output_root, o.checkout);
        });
    }
    r.run = p.summary;

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
                .fix = std::string(t.fixes[id])});
        }
    }
    std::ranges::stable_sort(r.findings, {}, [](const types::finding& f) {
        return std::tie(f.path, f.line, f.column);
    });
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

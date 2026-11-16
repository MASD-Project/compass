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
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include "compass.cli/app/steps.hpp"
#include "compass.codec/codecs/catalogue.hpp"
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
 * anything is written. The files and the corpus point into paths, so the
 * pipeline is never moved once evaluated.
 */
struct pipeline {
    std::vector<std::string> paths;
    corpus_files files;
    ct::corpus corpus;
    ct::registry codecs;
    ct::run_result run;
    types::run_summary summary;
};

/**
 * @brief Lists, loads and runs the codecs over the checkout of @p o into
 * @p p. Reads the checkout, the store, the dependency record and the
 * result cache, and writes nothing.
 */
void evaluate(pipeline& p, const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    auto& s = p.summary;
    const auto type_registry = build_type_registry(o.meta);

    p.paths = list(o.checkout);
    p.files = open_files(o.checkout,
        store::io::objects_dir(store::io::git_common_dir(o.checkout)),
        {p.paths.begin(), p.paths.end()});
    s.files = p.paths.size();
    resolve(p.files, o.threads);
    load(p.files, o.threads);

    for (std::size_t row = 0; row < p.paths.size(); ++row) {
        if (!p.files.errors[row].empty())
            s.errors.push_back(p.paths[row] + ": " + p.files.errors[row]);
        else
            ++(p.files.mapped[row] ? s.loaded : s.parsed);
    }
    p.corpus = corpus_of(p.files, type_registry);
    s.documents = p.corpus.size();

    p.codecs = codec::ops::make_registry(codec::codecs::catalogue(),
        type_registry, o.settings);
    s.problems = p.codecs.problems;
    const auto previous = read_dependency_record(o.checkout);
    p.run = run_codecs(p.codecs, p.corpus, type_registry, o.threads,
        previous.record, o.checkout);
    for (std::size_t c = 0; c < p.codecs.size(); ++c)
        s.codecs.emplace_back(p.codecs.codecs[c].name);
    s.ran = p.run.ran;
    s.ms = since(start);
}

/**
 * @brief Writes what @p p computed that later runs reuse: the objects it
 * parsed, the worktree index, the result cache and the dependency record.
 */
void persist(const pipeline& p, unsigned threads) {
    std::ignore = store_objects(p.files);
    std::ignore = write_worktree_index(p.files);
    std::ignore = store_results(p.files.checkout, p.run, threads);
    std::ignore = write_dependency_record(p.files.checkout, p.run);
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

}

types::generate_result generate(const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    types::generate_result r;
    r.root = o.output_root;
    pipeline p;
    evaluate(p, o);
    persist(p, o.threads);
    r.run = p.summary;
    if (!r.run.failed()) {
        r.sink = write_outputs(o.checkout, o.output_root, p.codecs, p.run)
            .sink;
    }
    r.ms = since(start);
    return r;
}

types::diff_result diff(const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    types::diff_result r;
    r.root = o.output_root;
    pipeline p;
    evaluate(p, o);
    r.run = p.summary;
    if (!r.run.failed()) {
        planned_outputs planned;
        plan_outputs(planned, o.checkout, o.output_root, p.codecs, p.run);
        r.planned = codec::io::summarise(planned.plan);
        r.diff = codec::io::diff_outputs(planned.plan);
    }
    r.ms = since(start);
    return r;
}

types::check_result check(const types::options& o) {
    const auto start = std::chrono::steady_clock::now();
    types::check_result r;
    pipeline p;
    evaluate(p, o);
    persist(p, o.threads);
    r.run = p.summary;

    for (std::size_t c = 0; c < p.codecs.size(); ++c) {
        const auto& d = p.codecs.codecs[c];
        const auto& t = p.run.diagnostics[c];
        for (std::uint32_t i = 0; i < t.size(); ++i) {
            const ct::diagnostic_id id{i};
            const auto document = t.documents[id].value;
            const auto [line, column] =
                position_of(p.corpus.sources[document], t.ranges[id].begin);
            r.findings.push_back({
                .path = std::string(p.corpus.paths[document]),
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

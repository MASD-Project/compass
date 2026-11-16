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
#include "compass.bench/ops/codecs_stage.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.cli/app/steps.hpp"
#include "compass.codec/codecs/catalogue.hpp"
#include "compass.codec/ops/registry.hpp"

namespace compass::bench::ops {

namespace {

namespace ot = compass::org::types;
namespace ct = compass::codec::types;
namespace app = compass::cli::app;

constexpr std::size_t top_count = 15;
constexpr std::size_t first_count = 10;

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

using text_counts = std::map<std::string, std::uint64_t, std::less<>>;

std::vector<types::counted_text> top(const text_counts& counts) {
    std::vector<types::counted_text> out;
    for (const auto& [text, count] : counts)
        out.push_back({text, count});
    std::ranges::stable_sort(out, std::greater{},
        &types::counted_text::count);
    if (out.size() > top_count)
        out.resize(top_count);
    return out;
}

std::size_t line_of(std::string_view source, std::uint32_t offset) {
    return 1 + static_cast<std::size_t>(
        std::ranges::count(source.substr(0, offset), '\n'));
}

types::findings_summary summarise(const ct::corpus& c,
    const ct::descriptor& d, const ct::diagnostic_table& t) {
    types::findings_summary s;
    text_counts rules;
    text_counts messages;
    auto previous = ot::document_id::none();
    for (std::uint32_t i = 0; i < t.size(); ++i) {
        const ct::diagnostic_id id{i};
        const auto document = t.documents[id];
        if (document != previous)
            ++s.documents;
        previous = document;
        const auto rule = d.rules[t.rules[id].value].name;
        ++rules[std::string(rule)];
        ++messages[std::string(t.messages[id])];
        if (s.first.size() < first_count)
            s.first.push_back(std::format("{}:{}: {}: {}/{}: {}. {}",
                c.paths[document.value],
                line_of(c.sources[document.value], t.ranges[id].begin),
                t.levels[id] == ct::severity::error ? "error" : "warning",
                d.name, rule, t.messages[id], t.fixes[id]));
    }
    s.rules = top(rules);
    s.messages = top(messages);
    return s;
}

/**
 * @brief What a run of the codecs read and computed, before anything is
 * written: the files the corpus points into, and the result.
 */
struct evaluation {
    app::corpus_files files;
    types::codecs_result result;
};

/**
 * @brief Runs the steps of compass.cli up to the codecs into @p e, timing
 * each. Reads the checkout, the store, the dependency record and the
 * result cache, and writes nothing: what a run stores stays in @p e for
 * persist.
 */
void evaluate(evaluation& e, const std::filesystem::path& checkout,
    const types::file_table& table, const std::filesystem::path& meta,
    const std::filesystem::path& objects,
    const codec::types::configuration& settings, unsigned threads,
    unsigned repetitions) {
    auto& r = e.result;
    auto& f = e.files;

    auto start = std::chrono::steady_clock::now();
    const auto registry = app::build_type_registry(meta);
    r.registry_ms = since(start);
    r.registered_types = registry.types.size();

    std::vector<std::string_view> paths;
    for (std::uint32_t id = 0; id < table.size(); ++id) {
        if (table.extensions[table.extension_ids[id]] == ".org")
            paths.emplace_back(table.paths[id]);
    }
    r.files = paths.size();

    start = std::chrono::steady_clock::now();
    f = app::open_files(checkout, objects, std::move(paths));
    app::resolve(f, threads);
    app::load(f, threads);
    const auto corpus = app::corpus_of(f, registry);
    r.load_ms = since(start);
    for (std::size_t row = 0; row < f.paths.size(); ++row) {
        if (f.errors[row].empty()) {
            ++(f.mapped[row] ? r.loaded : r.parsed);
        } else if (r.errors++ == 0) {
            r.first_error = std::string(f.paths[row]) + ": " + f.errors[row];
        }
    }
    r.documents = corpus.size();

    r.registry = codec::ops::make_registry(codec::codecs::catalogue(),
        registry, settings);
    start = std::chrono::steady_clock::now();
    const auto previous = app::read_dependency_record(checkout);
    r.record_read_ms = since(start);
    r.record_problem = previous.problem;
    r.record_bytes = previous.bytes;
    r.record_units = previous.record.size();

    r.ms.resize(r.registry.size());
    for (unsigned rep = 0; rep < repetitions; ++rep) {
        r.last = app::run_codecs(r.registry, corpus, registry, threads,
            previous.record, checkout);
        r.wall_ms.push_back(r.last.wall_ms);
        for (std::size_t c = 0; c < r.registry.size(); ++c)
            r.ms[c].push_back(r.last.ms[c]);
    }
    for (std::size_t c = 0; c < r.registry.size(); ++c)
        r.findings.push_back(summarise(corpus, r.registry.codecs[c],
            r.last.diagnostics[c]));
}

/**
 * @brief Writes what @p e computed: the objects it parsed, the worktree
 * index, the results of the units that ran to the result cache and the
 * dependency record, then, with an @p output_root, the outputs and the
 * written-outputs record. The only writer of the stage.
 */
void persist(evaluation& e, const std::filesystem::path& checkout,
    unsigned threads, unsigned repetitions,
    const std::filesystem::path& output_root) {
    auto& r = e.result;
    auto start = std::chrono::steady_clock::now();
    r.written = app::store_objects(e.files);
    app::write_worktree_index(e.files);
    r.load_ms += since(start);

    if (repetitions != 0) {
        start = std::chrono::steady_clock::now();
        r.actions = app::store_results(checkout, r.last, threads);
        r.actions_ms = since(start);
        start = std::chrono::steady_clock::now();
        r.record_written = app::write_dependency_record(checkout, r.last);
        r.record_write_ms = since(start);
    }
    if (repetitions != 0 && !output_root.empty()) {
        start = std::chrono::steady_clock::now();
        auto written =
            app::write_outputs(checkout, output_root, r.registry, r.last);
        r.outputs_problem = std::move(written.record_problem);
        r.sink = std::move(written.sink);
        r.wrote_outputs = r.sink.problems.empty();
        r.outputs_written = written.record_written;
        r.sink_ms = since(start);
    }
}

}

types::codecs_result run_codecs_stage(const std::filesystem::path& checkout,
    const types::file_table& table, const std::filesystem::path& meta,
    const std::filesystem::path& objects,
    const codec::types::configuration& settings, unsigned threads,
    unsigned repetitions, const std::filesystem::path& output_root) {
    evaluation e;
    evaluate(e, checkout, table, meta, objects, settings, threads,
        repetitions);
    persist(e, checkout, threads, repetitions, output_root);
    return std::move(e.result);
}

types::codecs_diff diff_codecs_stage(const std::filesystem::path& checkout,
    const types::file_table& table, const std::filesystem::path& meta,
    const std::filesystem::path& objects,
    const codec::types::configuration& settings, unsigned threads,
    unsigned repetitions, const std::filesystem::path& root) {
    evaluation e;
    evaluate(e, checkout, table, meta, objects, settings, threads,
        repetitions);
    types::codecs_diff d;
    auto start = std::chrono::steady_clock::now();
    app::planned_outputs planned;
    app::plan_outputs(planned, checkout, root, e.result.registry,
        e.result.last);
    e.result.outputs_problem = planned.previous.problem;
    d.plan_ms = since(start);
    d.planned = codec::io::summarise(planned.plan);
    start = std::chrono::steady_clock::now();
    d.diff = codec::io::diff_outputs(planned.plan);
    d.diff_ms = since(start);
    d.run = std::move(e.result);
    return d;
}

}

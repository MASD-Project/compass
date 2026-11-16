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
#include "compass.codec/ops/run.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>
#include "compass.codec/ops/dependencies.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.org/ops/type_lookup.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::codec::ops {

namespace {

namespace ot = compass::org::types;
using compass::store::types::blob_id;

/**
 * @brief How many pieces a document codec is split into per thread, so a
 * thread that drew cheap documents takes more of them.
 */
constexpr std::size_t pieces_per_thread = 4;

constexpr std::uint32_t no_unit = 0xFFFF'FFFF;

/**
 * @brief What the units of one codec are keyed by in this run: its option
 * values and, for a corpus codec, the set of documents it may read.
 */
struct codec_key {
    blob_id configuration;
    blob_id type_set;
};

/**
 * @brief The units of a run, grouped by codec in registry order and, for
 * a document codec, in visible-document order. visible is the unit's
 * document in its codec's view. previous is the unit's row in the previous
 * record when that row stands in for the call, or no_unit when the unit is
 * called.
 */
struct unit_table {
    std::vector<types::codec_id> codecs;
    std::vector<std::uint32_t> visible;
    std::vector<std::uint32_t> previous;

    [[nodiscard]] std::size_t size() const noexcept { return codecs.size(); }
};

/**
 * @brief Where the output of one called unit is: rows [files_begin,
 * files_end) and [diagnostics_begin, diagnostics_end) of the tables of
 * its piece.
 */
struct call_rows {
    std::uint32_t piece = 0;
    std::uint32_t files_begin = 0;
    std::uint32_t files_end = 0;
    std::uint32_t diagnostics_begin = 0;
    std::uint32_t diagnostics_end = 0;
};

/**
 * @brief Entries [begin, end) of the list of called units, all of one
 * codec, with the tables they fill.
 */
struct piece {
    types::codec_id codec;
    std::size_t begin = 0;
    std::size_t end = 0;
    types::file_table files;
    types::diagnostic_table diagnostics;
    double ms = 0;
};

template <typename Output>
Output& output(piece& p) noexcept {
    return std::get<Output&>(std::tie(p.files, p.diagnostics));
}

/**
 * @brief Calls the units of one piece, each with a log of its own.
 */
struct call {
    piece& p;
    std::uint32_t piece_index;
    types::corpus_view view;
    std::span<const std::uint32_t> called;
    const unit_table& units;
    std::vector<types::read_log>& logs;
    std::vector<call_rows>& rows;

    template <typename Output>
    void around(std::uint32_t unit, auto&& body) {
        view.record_to(&logs[unit]);
        auto& r = rows[unit];
        r.piece = piece_index;
        r.files_begin = static_cast<std::uint32_t>(p.files.size());
        r.diagnostics_begin = static_cast<std::uint32_t>(
            p.diagnostics.size());
        body(output<Output>(p));
        r.files_end = static_cast<std::uint32_t>(p.files.size());
        r.diagnostics_end = static_cast<std::uint32_t>(p.diagnostics.size());
    }

    template <typename Output>
    void operator()(types::document_function<Output> f) {
        for (auto i = p.begin; i < p.end; ++i) {
            const auto unit = called[i];
            around<Output>(unit, [&](Output& out) {
                f(view[units.visible[unit]], view, out);
            });
        }
    }

    template <typename Output>
    void operator()(types::corpus_function<Output> f) {
        const auto unit = called[p.begin];
        logs[unit].by_document = false;
        around<Output>(unit, [&](Output& out) { f(view, out); });
    }
};

std::vector<ot::document_id> visible_rows(const types::registry& r,
    types::codec_id codec, const types::corpus& c) {
    std::vector<ot::document_id> rows;
    for (std::uint32_t row = 0; row < c.size(); ++row) {
        if (reads(r, codec, c.types[row]))
            rows.push_back({row});
    }
    return rows;
}

std::span<const std::uint8_t> flags_of(const types::registry& r,
    types::codec_id codec) noexcept {
    return std::span<const std::uint8_t>(r.reads).subspan(
        codec.value * r.type_slots, r.type_slots);
}

template <typename Column>
std::uint32_t end_of(const Column& firsts, types::record_unit_id u,
    std::size_t total) noexcept {
    return u.value + 1 < firsts.size() ? firsts[u.next()].value
        : static_cast<std::uint32_t>(total);
}

std::string unit_name(std::string_view codec, std::string_view document) {
    std::string name(codec);
    name.push_back('\0');
    name += document;
    return name;
}

/**
 * @brief The current answer to recorded input @p in, as the codec whose
 * type flags are @p reads would get it.
 */
std::optional<blob_id> answer(const types::dependency_record& previous,
    types::input_id in, const types::corpus& c,
    const types::corpus_index& index, std::span<const std::uint8_t> reads,
    const codec_key& key, const blob_id& registry) {
    const auto kind = previous.input_kinds[in];
    switch (kind) {
    case types::input_kind::path:
    case types::input_kind::id: {
        const auto row = types::locate(c, index, reads, kind,
            previous.input_keys[in]);
        if (row.is_none())
            return std::nullopt;
        return types::answer_of(c, kind, row);
    }
    case types::input_kind::type_set: return key.type_set;
    case types::input_kind::type_registry: return registry;
    }
    return std::nullopt;
}

bool reusable(const types::dependency_record& previous,
    types::record_unit_id u, const types::descriptor& d,
    const codec_key& key, const types::corpus& c,
    const types::corpus_index& index, std::span<const std::uint8_t> reads,
    const blob_id& registry) {
    if (previous.versions[u] != d.version ||
        previous.configurations[u] != key.configuration)
        return false;
    const auto end = end_of(previous.input_firsts, u,
        previous.input_count());
    for (auto i = previous.input_firsts[u].value; i < end; ++i) {
        const types::input_id in{i};
        const auto now = answer(previous, in, c, index, reads, key,
            registry);
        if (now.has_value() != (previous.input_found[in] != 0) ||
            (now && *now != previous.input_values[in]))
            return false;
    }
    return decode_diagnostics(previous.diagnostics[u], index, nullptr);
}

void add_files(types::file_table& to, const types::file_table& from,
    std::uint32_t begin, std::uint32_t end) {
    for (auto f = begin; f < end; ++f)
        to.add(from.paths[types::file_id{f}],
            from.contents[types::file_id{f}]);
}

void add_diagnostics(types::diagnostic_table& to,
    const types::diagnostic_table& from, std::uint32_t begin,
    std::uint32_t end) {
    for (auto i = begin; i < end; ++i) {
        const types::diagnostic_id d{i};
        to.add({from.documents[d], from.ranges[d], from.rules[d],
            from.levels[d], from.messages[d], from.fixes[d]});
    }
}

void add_input(types::dependency_record& to, types::input_kind kind,
    std::string_view key, std::uint8_t found, const blob_id& value) {
    to.input_kinds.values.push_back(kind);
    to.input_keys.add(key);
    to.input_found.values.push_back(found);
    to.input_values.values.push_back(value);
}

void open_unit(types::dependency_record& to, std::string_view codec,
    std::uint32_t version, const blob_id& configuration,
    std::string_view document) {
    to.codecs.add(codec);
    to.versions.values.push_back(version);
    to.configurations.values.push_back(configuration);
    to.documents.add(document);
    to.input_firsts.values.push_back(
        {static_cast<std::uint32_t>(to.input_count())});
    to.file_firsts.values.push_back(
        {static_cast<std::uint32_t>(to.files.size())});
}

void copy_unit(types::dependency_record& to,
    const types::dependency_record& from, types::record_unit_id u) {
    open_unit(to, from.codecs[u], from.versions[u], from.configurations[u],
        from.documents[u]);
    const auto inputs_end = end_of(from.input_firsts, u, from.input_count());
    for (auto i = from.input_firsts[u].value; i < inputs_end; ++i) {
        const types::input_id in{i};
        add_input(to, from.input_kinds[in], from.input_keys[in],
            from.input_found[in], from.input_values[in]);
    }
    const auto files_end = end_of(from.file_firsts, u, from.files.size());
    for (auto f = from.file_firsts[u].value; f < files_end; ++f) {
        const types::file_id file{f};
        to.files.add(from.files.paths[file], from.files.contents[file]);
        to.file_hashes.values.push_back(from.file_hashes[file]);
    }
    to.diagnostic_counts.values.push_back(from.diagnostic_counts[u]);
    to.diagnostic_hashes.values.push_back(from.diagnostic_hashes[u]);
    to.diagnostics.add(from.diagnostics[u]);
}

void add_called_unit(types::dependency_record& to, const types::corpus& c,
    const types::descriptor& d, const codec_key& key, std::string_view
    document, const types::read_log& log, const piece& p,
    const call_rows& rows, const blob_id& registry) {
    open_unit(to, d.name, d.version, key.configuration, document);
    for (std::size_t i = 0; i < log.size(); ++i)
        add_input(to, log.kinds[i], log.keys[i], log.found[i],
            log.values[i]);
    if (!log.by_document)
        add_input(to, types::input_kind::type_set, {}, 1, key.type_set);
    if (log.registry)
        add_input(to, types::input_kind::type_registry, {}, 1, registry);
    for (auto f = rows.files_begin; f < rows.files_end; ++f) {
        const types::file_id file{f};
        const auto content = p.files.contents[file];
        to.files.add(p.files.paths[file], content);
        to.file_hashes.values.push_back(
            compass::store::ops::blob_id_of(content));
    }
    const auto findings = encode_diagnostics(c, p.diagnostics,
        rows.diagnostics_begin, rows.diagnostics_end);
    to.diagnostic_counts.values.push_back(
        rows.diagnostics_end - rows.diagnostics_begin);
    to.diagnostic_hashes.values.push_back(
        {compass::store::ops::sha1_of(findings)});
    to.diagnostics.add(findings);
}

}

ot::type_id document_type(std::string_view source,
    const ot::tables_view& tables, const ot::type_registry& types) noexcept {
    const auto k = org::ops::find_keyword(tables, "type");
    if (k.is_none())
        return ot::type_id::none();
    return org::ops::find_type(types,
        org::ops::trimmed(source, tables.keywords.values[k]).of(source));
}

types::run_result run(const types::registry& r, const types::corpus& c,
    const ot::type_registry& types, unsigned threads,
    const types::dependency_record& previous) {
    const auto start = std::chrono::steady_clock::now();
    threads = std::max(threads, 1u);

    types::run_result result;
    result.documents.assign(r.size(), 0);
    result.ran.assign(r.size(), 0);
    result.skipped.assign(r.size(), 0);
    result.files.resize(r.size());
    result.diagnostics.resize(r.size());
    result.ms.assign(r.size(), 0);

    const auto index = index_corpus(c);
    const auto registry = registry_hash(types);
    std::unordered_map<std::string, std::uint32_t> recorded;
    recorded.reserve(previous.size());
    for (std::uint32_t u = 0; u < previous.size(); ++u)
        recorded.emplace(unit_name(previous.codecs[{u}],
            previous.documents[{u}]), u);

    std::vector<std::vector<ot::document_id>> rows(r.size());
    std::vector<codec_key> keys(r.size());
    std::vector<types::corpus_view> views;
    views.reserve(r.size());
    unit_table units;
    for (std::uint32_t i = 0; i < r.size(); ++i) {
        const types::codec_id codec{i};
        const auto& d = r.codecs[i];
        const auto corpus_unit = unit_of(d) == types::unit::corpus;
        if (r.enabled[i] != 0)
            rows[i] = visible_rows(r, codec, c);
        views.emplace_back(c, types, index, rows[i], flags_of(r, codec),
            d.options, option_values(r, codec));
        if (r.enabled[i] == 0)
            continue;
        result.documents[i] = rows[i].size();
        keys[i].configuration = configuration_hash(r, codec);
        if (corpus_unit)
            keys[i].type_set = type_set_hash(c, rows[i]);

        const auto count = corpus_unit ? 1 : rows[i].size();
        for (std::size_t v = 0; v < count; ++v) {
            const auto document = corpus_unit ? std::string_view{}
                : c.paths[rows[i][v].value];
            auto previous_unit = no_unit;
            const auto it = recorded.find(unit_name(d.name, document));
            if (it != recorded.end() && reusable(previous,
                    {it->second}, d, keys[i], c, index, flags_of(r, codec),
                    registry))
                previous_unit = it->second;
            units.codecs.push_back(codec);
            units.visible.push_back(static_cast<std::uint32_t>(v));
            units.previous.push_back(previous_unit);
        }
    }

    std::vector<std::uint32_t> called;
    std::vector<piece> pieces;
    for (std::uint32_t u = 0; u < units.size();) {
        const auto codec = units.codecs[u];
        const auto first = called.size();
        for (; u < units.size() && units.codecs[u] == codec; ++u) {
            if (units.previous[u] == no_unit)
                called.push_back(u);
        }
        const auto count = called.size() - first;
        if (count == 0)
            continue;
        const auto parts = unit_of(r.codecs[codec.value]) ==
            types::unit::corpus ? 1 : std::min(count,
                threads * pieces_per_thread);
        for (std::size_t i = 0; i < parts; ++i)
            pieces.push_back({codec, first + count * i / parts,
                first + count * (i + 1) / parts, {}, {}, 0});
    }

    std::vector<types::read_log> logs(units.size());
    std::vector<call_rows> call_outputs(units.size());
    std::atomic<std::size_t> next{0};
    std::mutex failure_lock;
    std::exception_ptr failure;
    const auto work = [&] {
        for (auto i = next++; i < pieces.size(); i = next++) {
            auto& p = pieces[i];
            const auto began = std::chrono::steady_clock::now();
            try {
                std::visit(call{p, static_cast<std::uint32_t>(i),
                    views[p.codec.value], called, units, logs,
                    call_outputs}, r.codecs[p.codec.value].run);
            } catch (...) {
                const std::lock_guard lock(failure_lock);
                if (!failure)
                    failure = std::current_exception();
            }
            p.ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - began).count();
        }
    };
    {
        std::vector<std::jthread> workers;
        const auto count = std::min<std::size_t>(threads, pieces.size());
        workers.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            workers.emplace_back(work);
    }
    if (failure)
        std::rethrow_exception(failure);

    for (const auto& p : pieces)
        result.ms[p.codec.value] += p.ms;
    for (std::uint32_t u = 0; u < units.size(); ++u) {
        const auto codec = units.codecs[u];
        const auto& d = r.codecs[codec.value];
        auto& files = result.files[codec.value];
        auto& diagnostics = result.diagnostics[codec.value];
        if (units.previous[u] != no_unit) {
            const types::record_unit_id from{units.previous[u]};
            add_files(files, previous.files,
                previous.file_firsts[from].value,
                end_of(previous.file_firsts, from, previous.files.size()));
            (void)decode_diagnostics(previous.diagnostics[from], index,
                &diagnostics);
            copy_unit(result.record, previous, from);
            ++result.skipped[codec.value];
            continue;
        }
        const auto& at = call_outputs[u];
        const auto& p = pieces[at.piece];
        add_files(files, p.files, at.files_begin, at.files_end);
        add_diagnostics(diagnostics, p.diagnostics, at.diagnostics_begin,
            at.diagnostics_end);
        const auto document = unit_of(d) == types::unit::corpus
            ? std::string_view{}
            : c.paths[rows[codec.value][units.visible[u]].value];
        add_called_unit(result.record, c, d, keys[codec.value], document,
            logs[u], p, at, registry);
        ++result.ran[codec.value];
    }
    result.wall_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    return result;
}

}

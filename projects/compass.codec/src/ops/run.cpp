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
#include <chrono>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>
#include "compass.codec/ops/action_cache.hpp"
#include "compass.codec/ops/dependencies.hpp"
#include "compass.codec/ops/parallel_for.hpp"
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
 * record when that row stands in for the call, or no_unit. manifests holds
 * the unit's manifest key. seeds holds, for a scan, the key of the result
 * the previous record names for it under the same version and options,
 * whose scan table the call starts from, and nothing otherwise.
 */
struct unit_table {
    std::vector<types::codec_id> codecs;
    std::vector<std::uint32_t> visible;
    std::vector<std::uint32_t> previous;
    std::vector<blob_id> manifests;
    std::vector<std::optional<blob_id>> seeds;

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
 * @brief What a scan reads besides its view: the corpus and its index,
 * for the content IDs of files and the findings of its scan table, and
 * the result cache its seed comes from.
 */
struct scan_context {
    const types::corpus& c;
    const types::corpus_index& index;
    const types::action_source& cache;
};

/**
 * @brief Calls the scan @p f through @p view, mapping again only the files
 * whose content ID differs from the one the scan table in the result
 * object @p seed holds for them, and returns the scan table of the call.
 * A file taken from the table is a file input of @p log with its content
 * ID, as reading it would have been.
 */
std::string run_scan(const types::scan_function& f,
    const types::corpus_view& view, const scan_context& s,
    std::string_view seed, types::read_log& log,
    types::diagnostic_table& out) {
    std::unordered_map<std::string_view, scan_entry> before;
    if (const auto result = decode_result(seed)) {
        if (auto entries = decode_scan(result->scan))
            before = std::move(*entries);
    }
    const auto paths = f.files(view);
    std::vector<std::string> facts(paths.size());
    std::vector<types::scanned_file> files;
    files.reserve(paths.size());
    std::string table;
    for (std::size_t i = 0; i < paths.size(); ++i) {
        const auto path = paths[i];
        const auto id = s.c.files != nullptr ? s.c.files->content_of(path)
            : std::nullopt;
        const auto it = id ? before.find(path) : before.end();
        if (it != before.end() && it->second.content == *id &&
            decode_diagnostics(it->second.findings, s.index, nullptr)) {
            log.add(types::input_kind::file, path, id);
            (void)decode_diagnostics(it->second.findings, s.index, &out);
            put_scan_entry(table, path, it->second);
            files.push_back({path, it->second.fact});
            continue;
        }
        const auto bytes = view.read_file(path);
        types::file_scan mapped;
        f.map(path, bytes ? std::string_view(*bytes) : std::string_view{},
            view.options(), mapped);
        facts[i] = std::move(mapped.fact);
        const auto findings = encode_diagnostics(s.c, mapped.findings, 0,
            static_cast<std::uint32_t>(mapped.findings.size()));
        out.append(mapped.findings);
        if (bytes)
            put_scan_entry(table, path, {store::ops::blob_id_of(*bytes),
                facts[i], findings});
        files.push_back({path, facts[i]});
    }
    if (f.reduce != nullptr)
        f.reduce(files, view, out);
    return table;
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
    const scan_context& scanning;
    std::vector<std::string>& scans;

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
        logs[unit].corpus = true;
        around<Output>(unit, [&](Output& out) { f(view, out); });
    }

    void operator()(const types::scan_function& f) {
        const auto unit = called[p.begin];
        logs[unit].corpus = true;
        std::string seed;
        if (units.seeds[unit] && scanning.cache) {
            if (auto body = scanning.cache(store::types::action_kind::result,
                    *units.seeds[unit]))
                seed = std::move(*body);
        }
        around<types::diagnostic_table>(unit,
            [&](types::diagnostic_table& out) {
                scans[unit] = run_scan(f, view, scanning, seed,
                    logs[unit], out);
            });
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
std::uint32_t end_of(const Column& firsts, std::uint32_t row,
    std::size_t total) noexcept {
    return row + 1 < firsts.size() ? firsts.values[row + 1].value
        : static_cast<std::uint32_t>(total);
}

std::string unit_name(std::string_view codec, std::string_view document) {
    std::string name(codec);
    name.push_back('\0');
    name += document;
    return name;
}

/**
 * @brief What the current corpus says to the inputs of one codec: the
 * answers a call would get, without calling it.
 */
struct answers {
    const types::corpus& c;
    const types::corpus_index& index;
    std::span<const std::uint8_t> reads;
    const codec_key& key;
    const blob_id& registry;

    [[nodiscard]] std::optional<blob_id> of(types::input_kind kind,
        std::string_view input) const {
        switch (kind) {
        case types::input_kind::path:
        case types::input_kind::id: {
            const auto row = types::locate(c, index, reads, kind, input);
            if (row.is_none())
                return std::nullopt;
            return types::answer_of(c, kind, row);
        }
        case types::input_kind::type_set: return key.type_set;
        case types::input_kind::type_registry: return registry;
        case types::input_kind::file:
        case types::input_kind::file_exists:
        case types::input_kind::listing:
            return types::file_answer(c, kind, input);
        }
        return std::nullopt;
    }

    /**
     * @brief True when every input in [begin, end) of @p t, a record or a
     * manifest, still has the answer it recorded, misses included.
     */
    template <typename Table>
    [[nodiscard]] bool hold(const Table& t, std::uint32_t begin,
        std::uint32_t end) const {
        for (auto i = begin; i < end; ++i) {
            const types::input_id in{i};
            const auto now = of(t.input_kinds[in], t.input_keys[in]);
            if (now.has_value() != (t.input_found[in] != 0) ||
                (now && *now != t.input_values[in]))
                return false;
        }
        return true;
    }
};

bool reusable(const types::dependency_record& previous,
    types::record_unit_id u, const types::descriptor& d,
    const answers& now) {
    if (previous.versions[u] != d.version ||
        previous.configurations[u] != now.key.configuration)
        return false;
    return now.hold(previous, previous.input_firsts[u].value,
               end_of(previous.input_firsts, u.value,
                   previous.input_count())) &&
        decode_diagnostics(previous.diagnostics[u], now.index, nullptr);
}

/**
 * @brief A unit whose result the cache holds: its manifest, the input set
 * of it that matched, and the result object that set names.
 */
struct cache_hit {
    blob_id manifest_key;
    types::action_manifest manifest;
    std::uint32_t set = 0;
    std::string body;
    action_result result;
};

/**
 * @brief The cached result of the unit whose manifest key is @p key: that
 * of the first input set of its manifest whose inputs all hold, if its
 * result object loads and names only documents of the corpus.
 */
std::optional<cache_hit> look_up(const types::action_source& cache,
    const blob_id& key, const answers& now) {
    const auto body = cache(store::types::action_kind::manifest, key);
    if (!body)
        return std::nullopt;
    auto manifest = decode_manifest(*body);
    if (!manifest)
        return std::nullopt;
    for (std::uint32_t s = 0; s < manifest->size(); ++s) {
        if (!now.hold(*manifest, manifest->input_firsts.values[s].value,
                end_of(manifest->input_firsts, s, manifest->input_count())))
            continue;
        auto bytes = cache(store::types::action_kind::result,
            manifest->results.values[s]);
        if (!bytes)
            return std::nullopt;
        cache_hit hit{key, std::move(*manifest), s, std::move(*bytes), {}};
        auto decoded = decode_result(hit.body);
        if (!decoded || !decode_diagnostics(decoded->diagnostics, now.index,
                nullptr))
            return std::nullopt;
        hit.result = std::move(*decoded);
        return hit;
    }
    return std::nullopt;
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
        to.add(from[types::diagnostic_id{i}]);
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

void add_record_files(types::dependency_record& to,
    const types::file_table& files, std::uint32_t begin, std::uint32_t end) {
    for (auto f = begin; f < end; ++f) {
        const types::file_id file{f};
        const auto content = files.contents[file];
        to.files.add(files.paths[file], content);
        to.file_hashes.values.push_back(
            compass::store::ops::blob_id_of(content));
    }
}

void add_record_findings(types::dependency_record& to,
    std::string_view findings, std::uint32_t count) {
    to.diagnostic_counts.values.push_back(count);
    to.diagnostic_hashes.values.push_back(
        {compass::store::ops::sha1_of(findings)});
    to.diagnostics.add(findings);
}

void copy_unit(types::dependency_record& to,
    const types::dependency_record& from, types::record_unit_id u) {
    open_unit(to, from.codecs[u], from.versions[u], from.configurations[u],
        from.documents[u]);
    const auto inputs_end = end_of(from.input_firsts, u.value,
        from.input_count());
    for (auto i = from.input_firsts[u].value; i < inputs_end; ++i) {
        const types::input_id in{i};
        add_input(to, from.input_kinds[in], from.input_keys[in],
            from.input_found[in], from.input_values[in]);
    }
    const auto files_end = end_of(from.file_firsts, u.value,
        from.files.size());
    for (auto f = from.file_firsts[u].value; f < files_end; ++f) {
        const types::file_id file{f};
        to.files.add(from.files.paths[file], from.files.contents[file]);
        to.file_hashes.values.push_back(from.file_hashes[file]);
    }
    to.diagnostic_counts.values.push_back(from.diagnostic_counts[u]);
    to.diagnostic_hashes.values.push_back(from.diagnostic_hashes[u]);
    to.diagnostics.add(from.diagnostics[u]);
    to.manifests.values.push_back(from.manifests[u]);
    to.results.values.push_back(from.results[u]);
}

/**
 * @brief Records the unit a cache hit stands in for as its call would
 * have recorded it: the matched input set is what the call would read.
 */
void add_cached_unit(types::dependency_record& to,
    const types::descriptor& d, const codec_key& key,
    std::string_view document, const cache_hit& hit,
    std::uint32_t findings) {
    open_unit(to, d.name, d.version, key.configuration, document);
    const auto& m = hit.manifest;
    const auto end = end_of(m.input_firsts, hit.set, m.input_count());
    for (auto i = m.input_firsts.values[hit.set].value; i < end; ++i) {
        const types::input_id in{i};
        add_input(to, m.input_kinds[in], m.input_keys[in], m.input_found[in],
            m.input_values[in]);
    }
    add_record_files(to, hit.result.files, 0,
        static_cast<std::uint32_t>(hit.result.files.size()));
    add_record_findings(to, hit.result.diagnostics, findings);
    to.manifests.values.push_back(hit.manifest_key);
    to.results.values.push_back(m.results.values[hit.set]);
}

void add_called_unit(types::dependency_record& to, const types::corpus& c,
    const types::descriptor& d, const codec_key& key, std::string_view
    document, const blob_id& manifest, const types::read_log& log,
    const piece& p, const call_rows& rows, const blob_id& registry) {
    const types::record_unit_id unit{static_cast<std::uint32_t>(to.size())};
    open_unit(to, d.name, d.version, key.configuration, document);
    for (std::size_t i = 0; i < log.size(); ++i)
        add_input(to, log.kinds[i], log.keys[i], log.found[i],
            log.values[i]);
    if (log.whole)
        add_input(to, types::input_kind::type_set, {}, 1, key.type_set);
    if (log.registry)
        add_input(to, types::input_kind::type_registry, {}, 1, registry);
    add_record_files(to, p.files, rows.files_begin, rows.files_end);
    add_record_findings(to, encode_diagnostics(c, p.diagnostics,
        rows.diagnostics_begin, rows.diagnostics_end),
        rows.diagnostics_end - rows.diagnostics_begin);
    to.manifests.values.push_back(manifest);
    to.results.values.push_back(result_key(manifest, to, unit));
}

double since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
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
    const types::dependency_record& previous,
    const types::action_source& cache, const unit_filter& only) {
    const auto start = std::chrono::steady_clock::now();
    threads = std::max(threads, 1u);

    types::run_result result;
    result.documents.assign(r.size(), 0);
    result.ran.assign(r.size(), 0);
    result.skipped.assign(r.size(), 0);
    result.cached.assign(r.size(), 0);
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
    const auto answers_of = [&](types::codec_id codec) {
        return answers{c, index, flags_of(r, codec), keys[codec.value],
            registry};
    };
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
        const auto count = corpus_unit ? 1 : rows[i].size();
        std::vector<std::uint32_t> taken;
        for (std::uint32_t v = 0; v < count; ++v) {
            if (!only || only(codec, corpus_unit ? std::string_view{}
                    : c.paths[rows[i][v].value]))
                taken.push_back(v);
        }
        if (taken.empty())
            continue;
        keys[i].configuration = configuration_hash(r, codec);
        if (corpus_unit)
            keys[i].type_set = type_set_hash(c, rows[i]);
        const auto now = answers_of(codec);

        for (const auto v : taken) {
            const auto document = corpus_unit ? std::string_view{}
                : c.paths[rows[i][v].value];
            const auto subject = corpus_unit ? blob_id{}
                : c.contents[rows[i][v].value];
            auto previous_unit = no_unit;
            const auto it = recorded.find(unit_name(d.name, document));
            if (it != recorded.end() &&
                reusable(previous, {it->second}, d, now))
                previous_unit = it->second;
            units.codecs.push_back(codec);
            units.visible.push_back(v);
            units.previous.push_back(previous_unit);
            units.manifests.push_back(manifest_key(d.name, d.version,
                keys[i].configuration, document, subject));
            std::optional<blob_id> seed;
            if (std::holds_alternative<types::scan_function>(d.run) &&
                it != recorded.end() &&
                previous.versions[{it->second}] == d.version &&
                previous.configurations[{it->second}] ==
                    keys[i].configuration)
                seed = previous.results[{it->second}];
            units.seeds.push_back(seed);
        }
    }

    const auto lookup_start = std::chrono::steady_clock::now();
    std::vector<std::optional<cache_hit>> hits(units.size());
    if (cache) {
        std::vector<std::uint32_t> unknown;
        for (std::uint32_t u = 0; u < units.size(); ++u) {
            if (units.previous[u] == no_unit)
                unknown.push_back(u);
        }
        parallel_for(unknown.size(), threads, [&](std::size_t i) {
            const auto u = unknown[i];
            hits[u] = look_up(cache, units.manifests[u],
                answers_of(units.codecs[u]));
        });
    }
    result.lookup_ms = since(lookup_start);

    std::vector<std::uint32_t> called;
    std::vector<piece> pieces;
    for (std::uint32_t u = 0; u < units.size();) {
        const auto codec = units.codecs[u];
        const auto first = called.size();
        for (; u < units.size() && units.codecs[u] == codec; ++u) {
            if (units.previous[u] == no_unit && !hits[u])
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
    std::vector<std::string> scans(units.size());
    const scan_context scanning{c, index, cache};
    parallel_for(pieces.size(), threads, [&](std::size_t i) {
        auto& p = pieces[i];
        const auto began = std::chrono::steady_clock::now();
        std::visit(call{p, static_cast<std::uint32_t>(i),
            views[p.codec.value], called, units, logs, call_outputs,
            scanning, scans}, r.codecs[p.codec.value].run);
        p.ms = since(began);
    });

    for (const auto& p : pieces)
        result.ms[p.codec.value] += p.ms;
    for (std::uint32_t u = 0; u < units.size(); ++u) {
        const auto codec = units.codecs[u];
        const auto& d = r.codecs[codec.value];
        auto& files = result.files[codec.value];
        auto& diagnostics = result.diagnostics[codec.value];
        const auto document = unit_of(d) == types::unit::corpus
            ? std::string_view{}
            : c.paths[rows[codec.value][units.visible[u]].value];
        if (units.previous[u] != no_unit) {
            const types::record_unit_id from{units.previous[u]};
            add_files(files, previous.files,
                previous.file_firsts[from].value,
                end_of(previous.file_firsts, from.value,
                    previous.files.size()));
            (void)decode_diagnostics(previous.diagnostics[from], index,
                &diagnostics);
            copy_unit(result.record, previous, from);
            ++result.skipped[codec.value];
            continue;
        }
        if (const auto& hit = hits[u]) {
            add_files(files, hit->result.files, 0,
                static_cast<std::uint32_t>(hit->result.files.size()));
            const auto before = diagnostics.size();
            (void)decode_diagnostics(hit->result.diagnostics, index,
                &diagnostics);
            add_cached_unit(result.record, d, keys[codec.value], document,
                *hit, static_cast<std::uint32_t>(diagnostics.size() -
                    before));
            ++result.cached[codec.value];
            continue;
        }
        const auto& at = call_outputs[u];
        const auto& p = pieces[at.piece];
        add_files(files, p.files, at.files_begin, at.files_end);
        add_diagnostics(diagnostics, p.diagnostics, at.diagnostics_begin,
            at.diagnostics_end);
        result.fresh.push_back(
            {static_cast<std::uint32_t>(result.record.size())});
        result.scans.push_back(std::move(scans[u]));
        add_called_unit(result.record, c, d, keys[codec.value], document,
            units.manifests[u], logs[u], p, at, registry);
        ++result.ran[codec.value];
    }
    const auto skipped = std::ranges::fold_left(result.skipped,
        std::uint64_t{0}, std::plus<>{});
    result.same_record = skipped == units.size() &&
        previous.size() == units.size();
    result.wall_ms = since(start);
    return result;
}

}

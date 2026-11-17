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
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
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
 * @brief What a unit is a call of: a document codec for one document, a
 * corpus codec or a scan, the prepare step of a staged codec, or one of
 * its parts.
 */
enum class unit_kind : std::uint8_t { document, corpus, stage, part };

/**
 * @brief The units of a run, grouped by codec in registry order and, for
 * a document codec, in visible-document order. visible is the unit's
 * document in its codec's view, or for a part its row among the parts its
 * prepare step listed. last is, for a part or a scan, its row in the
 * previous record under the same version and options when that row does
 * not stand in for the call, or no_unit, and changed, for a part, the
 * inputs of that row that no longer hold. previous is the unit's row in the previous
 * record when that row stands in for the call, or no_unit. manifests holds
 * the unit's manifest key. seeds holds, for a scan, the key of the result
 * the previous record names for it under the same version and options,
 * whose scan table the call starts from, and nothing otherwise.
 */
struct unit_table {
    std::vector<types::codec_id> codecs;
    std::vector<unit_kind> kinds;
    std::vector<std::uint32_t> visible;
    std::vector<std::uint32_t> previous;
    std::vector<blob_id> manifests;
    std::vector<std::optional<blob_id>> seeds;
    std::vector<std::uint32_t> last;
    std::vector<std::vector<types::changed_input>> changed;

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
 * codec, with the tables they fill. For the parts of a staged codec, ids
 * holds the blob ID of each row of files, and kept whether a part kept
 * the row from its last call, whose bytes may not be loaded.
 */
struct piece {
    types::codec_id codec;
    std::size_t begin = 0;
    std::size_t end = 0;
    types::file_table files;
    types::diagnostic_table diagnostics;
    double ms = 0;
    std::vector<blob_id> ids;
    std::vector<std::uint8_t> kept;
};

/**
 * @brief Calls @p body with the tables of @p p a call emitting @p Output
 * fills: the files and the refusals, or the diagnostics.
 */
template <typename Output>
void with_output(piece& p, auto&& body) {
    if constexpr (std::is_same_v<Output, types::file_output>) {
        types::file_output out{p.files, p.diagnostics};
        body(out);
    } else {
        body(p.diagnostics);
    }
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
 * @brief What the prepare step of one staged codec gave this run: the
 * parts and the facts, by name, and the state the parts run from, which
 * is null until prepare is called in this run. called is true when
 * prepare was called for the record, with its reads in log and its time
 * in ms.
 */
struct staging {
    types::stage stage;
    std::unordered_map<std::string_view, blob_id> facts;
    std::shared_ptr<const void> state;
    bool called = false;
    types::read_log log;
    double ms = 0;

    void index() {
        facts.clear();
        for (const auto& [name, value] : stage.facts)
            facts.emplace(name, value);
    }
};

/**
 * @brief What the current corpus says to the inputs of one codec: the
 * answers a call would get, without calling it. facts holds what the
 * prepare step of a staged codec published, and is null otherwise. edited
 * is a path whose inputs hold checks first.
 */
struct answers {
    const types::corpus& c;
    const types::corpus_index& index;
    std::span<const std::uint8_t> reads;
    const codec_key& key;
    const blob_id& registry;
    const std::unordered_map<std::string_view, blob_id>* facts = nullptr;
    std::string_view edited = {};

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
        case types::input_kind::fact: {
            if (facts == nullptr)
                return std::nullopt;
            const auto it = facts->find(input);
            if (it == facts->end())
                return std::nullopt;
            return it->second;
        }
        }
        return std::nullopt;
    }

    /**
     * @brief True when input @p in of @p t, a record or a manifest, still
     * has the answer it recorded, a miss included.
     */
    template <typename Table>
    [[nodiscard]] bool holds(const Table& t, types::input_id in) const {
        const auto now = of(t.input_kinds[in], t.input_keys[in]);
        return now.has_value() == (t.input_found[in] != 0) &&
            (!now || *now == t.input_values[in]);
    }

    template <typename Table>
    [[nodiscard]] bool hold(const Table& t, std::uint32_t begin,
        std::uint32_t end) const {
        for (auto i = begin; i < end && !edited.empty(); ++i) {
            if (t.input_keys[types::input_id{i}] == edited &&
                !holds(t, types::input_id{i}))
                return false;
        }
        for (auto i = begin; i < end; ++i) {
            if (!holds(t, types::input_id{i}))
                return false;
        }
        return true;
    }
};

template <typename Column>
std::uint32_t end_of(const Column& firsts, std::uint32_t row,
    std::size_t total) noexcept {
    return row + 1 < firsts.size() ? firsts.values[row + 1].value
        : static_cast<std::uint32_t>(total);
}

/**
 * @brief The row of a unit in the previous record, under the same version
 * and options, with what the corpus answers its inputs now; unit is
 * no_unit when the record has no such row.
 */
struct last_call {
    const types::dependency_record& record;
    std::uint32_t unit;
    const answers& now;
};

/**
 * @brief True when the reduce step of a scan would see what its last call
 * saw: every file mapped again in @p mapped has the fact @p before holds
 * for it and was not read by that reduce step, no file came or went, and
 * every other input of the last call still holds.
 */
bool reduces_the_same(const scan_table& before,
    std::span<const std::pair<std::string_view, scan_entry>> entries,
    std::span<const std::size_t> mapped, const last_call& last) {
    if (last.unit == no_unit || entries.size() != before.entries.size())
        return false;
    for (const auto i : mapped) {
        const auto it = before.entries.find(entries[i].first);
        if (it == before.entries.end() || it->second.read_by_reduce ||
            it->second.fact != entries[i].second.fact)
            return false;
    }
    const auto& r = last.record;
    const auto end = end_of(r.input_firsts, last.unit, r.input_count());
    for (auto i = r.input_firsts[{last.unit}].value; i < end; ++i) {
        const types::input_id in{i};
        if (r.input_kinds[in] == types::input_kind::file &&
            before.entries.contains(r.input_keys[in]))
            continue;
        if (!last.now.holds(r, in))
            return false;
    }
    return true;
}

/**
 * @brief Adds every input of @p from to @p to, a log of a corpus unit.
 */
void add_reads(types::read_log& to, const types::read_log& from) {
    for (std::size_t i = 0; i < from.size(); ++i)
        to.add(from.kinds[i], from.keys[i], from.found[i] != 0
            ? std::optional(from.values[i]) : std::nullopt);
    to.whole = to.whole || from.whole;
    to.registry = to.registry || from.registry;
}

/**
 * @brief Calls the scan @p f through @p view, mapping again only the files
 * whose content ID differs from the one the scan table in the result
 * object @p seed holds for them, and returns the scan table of the call.
 * A file taken from the table is a file input of @p log with its content
 * ID, as reading it would have been.
 *
 * The reduce step is not called when it would see what the call of
 * @p last saw: its findings are those the table holds and its inputs
 * those that call recorded. Otherwise it is called with a log of its
 * own, which says which of the files it read itself.
 */
std::string run_scan(const types::scan_function& f, types::corpus_view& view,
    const scan_context& s, std::string_view seed, const last_call& last,
    types::read_log& log, types::diagnostic_table& out) {
    scan_table before;
    bool seeded = false;
    if (const auto result = decode_result(seed)) {
        if (auto table = decode_scan(result->scan)) {
            before = std::move(*table);
            seeded = true;
        }
    }
    const auto paths = f.files(view);
    std::vector<std::string> facts(paths.size());
    std::vector<std::string> findings(paths.size());
    std::vector<types::scanned_file> files;
    files.reserve(paths.size());
    std::vector<std::pair<std::string_view, scan_entry>> entries;
    entries.reserve(paths.size());
    std::vector<std::size_t> mapped;
    bool unread = false;
    for (std::size_t i = 0; i < paths.size(); ++i) {
        const auto path = paths[i];
        const auto id = s.c.files != nullptr ? s.c.files->content_of(path)
            : std::nullopt;
        const auto it = id ? before.entries.find(path)
            : before.entries.end();
        if (it != before.entries.end() && it->second.content == *id &&
            decode_diagnostics(it->second.findings, s.index, nullptr)) {
            log.add(types::input_kind::file, path, id);
            (void)decode_diagnostics(it->second.findings, s.index, &out);
            entries.emplace_back(path, it->second);
            files.push_back({path, it->second.fact});
            continue;
        }
        const auto bytes = view.read_file(path);
        types::file_scan made;
        f.map(path, bytes ? std::string_view(*bytes) : std::string_view{},
            view.options(), made);
        facts[i] = std::move(made.fact);
        findings[i] = encode_diagnostics(s.c, made.findings, 0,
            static_cast<std::uint32_t>(made.findings.size()));
        out.append(made.findings);
        files.push_back({path, facts[i]});
        // A file that cannot be read has no entry, so it is mapped again
        // and the reduce step is called.
        unread = unread || !bytes;
        if (!bytes)
            continue;
        mapped.push_back(entries.size());
        entries.emplace_back(path, scan_entry{store::ops::blob_id_of(*bytes),
            facts[i], findings[i], false});
    }

    std::string reduced;
    if (seeded && !unread &&
        reduces_the_same(before, entries, mapped, last) &&
        decode_diagnostics(before.reduce_findings, s.index, nullptr)) {
        (void)decode_diagnostics(before.reduce_findings, s.index, &out);
        reduced = before.reduce_findings;
        for (auto& [path, e] : entries)
            e.read_by_reduce = before.entries.find(path)->second
                .read_by_reduce;
        const auto& r = last.record;
        const auto end = end_of(r.input_firsts, last.unit, r.input_count());
        for (auto i = r.input_firsts[{last.unit}].value; i < end; ++i) {
            const types::input_id in{i};
            const auto kind = r.input_kinds[in];
            if (kind == types::input_kind::type_set)
                log.whole = true;
            else if (kind == types::input_kind::type_registry)
                log.registry = true;
            else if (kind != types::input_kind::file ||
                !before.entries.contains(r.input_keys[in]))
                log.add(kind, r.input_keys[in], r.input_found[in] != 0
                    ? std::optional(r.input_values[in]) : std::nullopt);
        }
    } else if (f.reduce != nullptr) {
        types::read_log reads;
        reads.corpus = true;
        view.record_to(&reads);
        const auto first = static_cast<std::uint32_t>(out.size());
        f.reduce(files, view, out);
        view.record_to(&log);
        reduced = encode_diagnostics(s.c, out, first,
            static_cast<std::uint32_t>(out.size()));
        std::unordered_map<std::string_view, std::size_t> rows;
        for (std::size_t i = 0; i < reads.size(); ++i) {
            if (reads.kinds[i] != types::input_kind::file &&
                reads.kinds[i] != types::input_kind::path)
                continue;
            if (rows.empty()) {
                rows.reserve(entries.size());
                for (std::size_t e = 0; e < entries.size(); ++e)
                    rows.emplace(entries[e].first, e);
            }
            if (const auto it = rows.find(reads.keys[i]); it != rows.end())
                entries[it->second].second.read_by_reduce = true;
        }
        add_reads(log, reads);
    }
    return encode_scan(entries, reduced);
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
    const types::dependency_record& previous;
    const staging* staged;
    const answers now;

    template <typename Output>
    void around(std::uint32_t unit, auto&& body) {
        view.record_to(&logs[unit]);
        auto& r = rows[unit];
        r.piece = piece_index;
        r.files_begin = static_cast<std::uint32_t>(p.files.size());
        r.diagnostics_begin = static_cast<std::uint32_t>(
            p.diagnostics.size());
        with_output<Output>(p, body);
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
        const last_call last{previous, units.last[unit], now};
        around<types::diagnostic_table>(unit,
            [&](types::diagnostic_table& out) {
                scans[unit] = run_scan(f, view, scanning, seed, last,
                    logs[unit], out);
            });
    }

    /**
     * @brief The sink of the parts of this piece, begun in order.
     */
    class sink final : public types::part_sink {
    public:
        sink(call& c, std::span<const std::string_view> parts)
            : c_(c), parts_(parts) {}

        void begin(std::string_view part) override {
            close();
            if (next_ == parts_.size() || parts_[next_] != part)
                throw std::logic_error("a staged codec began the part " +
                    std::string(part) + " out of turn");
            unit_ = c_.called[c_.p.begin + next_++];
            inherited_ = false;
            c_.logs[unit_].corpus = true;
            c_.view.record_to(&c_.logs[unit_]);
            auto& r = c_.rows[unit_];
            r.piece = c_.piece_index;
            r.files_begin = static_cast<std::uint32_t>(c_.p.files.size());
        }

        void read(std::string_view fact) override {
            const auto it = c_.staged->facts.find(fact);
            c_.logs[at()].add(types::input_kind::fact, fact,
                it == c_.staged->facts.end() ? std::nullopt
                    : std::optional(it->second));
        }

        void add(std::string_view path, std::string_view bytes) override {
            (void)at();
            c_.p.files.add(path, bytes);
            c_.p.ids.emplace_back();
            c_.p.kept.push_back(0);
        }

        [[nodiscard]] std::optional<std::span<const types::changed_input>>
        changed() const override {
            if (c_.units.last[at()] == no_unit)
                return std::nullopt;
            return std::span(c_.units.changed[unit_]);
        }

        bool keep(std::string_view path) override {
            const auto last = c_.units.last[at()];
            if (last == no_unit)
                return false;
            const auto& from = c_.previous;
            const auto end = end_of(from.file_firsts, last,
                from.files.size());
            for (auto f = from.file_firsts[{last}].value; f < end; ++f) {
                const types::file_id file{f};
                if (from.files.paths[file] != path)
                    continue;
                inherit(last);
                c_.p.ids.push_back(from.file_hashes[file]);
                c_.p.kept.push_back(1);
                c_.p.files.add(path, from.files.contents[file]);
                return true;
            }
            return false;
        }

        void finish() {
            close();
            if (next_ != parts_.size())
                throw std::logic_error("a staged codec left the part " +
                    std::string(parts_[next_]) + " out");
        }

    private:
        [[nodiscard]] std::uint32_t at() const {
            if (unit_ == no_unit)
                throw std::logic_error("a staged codec emitted before it "
                    "began a part");
            return unit_;
        }

        void close() {
            if (unit_ != no_unit)
                c_.rows[unit_].files_end =
                    static_cast<std::uint32_t>(c_.p.files.size());
        }

        /**
         * @brief Adds every input the last call recorded to the log, with
         * the answer it has now.
         */
        void inherit(std::uint32_t last) {
            if (inherited_)
                return;
            inherited_ = true;
            const auto& from = c_.previous;
            auto& log = c_.logs[unit_];
            const auto end = end_of(from.input_firsts, last,
                from.input_count());
            for (auto i = from.input_firsts[{last}].value; i < end; ++i) {
                const types::input_id in{i};
                const auto kind = from.input_kinds[in];
                if (kind == types::input_kind::type_set)
                    log.whole = true;
                else if (kind == types::input_kind::type_registry)
                    log.registry = true;
                else
                    log.add(kind, from.input_keys[in],
                        c_.now.of(kind, from.input_keys[in]));
            }
        }

        call& c_;
        std::span<const std::string_view> parts_;
        std::size_t next_ = 0;
        std::uint32_t unit_ = no_unit;
        bool inherited_ = false;
    };

    void operator()(const types::staged_function& f) {
        std::vector<std::string_view> parts;
        for (auto i = p.begin; i < p.end; ++i)
            parts.emplace_back(
                staged->stage.parts[units.visible[called[i]]]);
        sink out(*this, parts);
        f.run(staged->state.get(), parts, view, out);
        out.finish();
        for (std::uint32_t row = 0; row < p.files.size(); ++row) {
            if (p.kept[row] == 0)
                p.ids[row] = store::ops::blob_id_of(
                    p.files.contents[types::file_id{row}]);
        }
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

std::string unit_name(std::string_view codec, std::string_view document) {
    std::string name(codec);
    name.push_back('\0');
    name += document;
    return name;
}

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

/**
 * @brief Adds rows [begin, end) of @p files to @p to with the blob ID of
 * each: that of its row in @p known when given, or else hashed on up to
 * @p threads threads, as a unit may hold many megabytes of files.
 */
void add_record_files(types::dependency_record& to,
    const types::file_table& files, std::uint32_t begin, std::uint32_t end,
    unsigned threads, std::span<const blob_id> known = {}) {
    std::vector<blob_id> ids(end - begin);
    const auto hash = [&](std::size_t i) {
        ids[i] = compass::store::ops::blob_id_of(
            files.contents[types::file_id{static_cast<std::uint32_t>(
                begin + i)}]);
    };
    if (!known.empty()) {
        std::ranges::copy(known.subspan(begin, end - begin), ids.begin());
    } else if (ids.size() < 256) {
        for (std::size_t i = 0; i < ids.size(); ++i)
            hash(i);
    } else {
        parallel_for(ids.size(), threads, hash);
    }
    for (auto f = begin; f < end; ++f) {
        const types::file_id file{f};
        to.files.add(files.paths[file], files.contents[file]);
        to.file_hashes.values.push_back(ids[f - begin]);
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
    std::uint32_t findings, unsigned threads) {
    open_unit(to, d.name, d.version, key.configuration, document);
    const auto& m = hit.manifest;
    const auto end = end_of(m.input_firsts, hit.set, m.input_count());
    for (auto i = m.input_firsts.values[hit.set].value; i < end; ++i) {
        const types::input_id in{i};
        add_input(to, m.input_kinds[in], m.input_keys[in], m.input_found[in],
            m.input_values[in]);
    }
    add_record_files(to, hit.result.files, 0,
        static_cast<std::uint32_t>(hit.result.files.size()), threads);
    add_record_findings(to, hit.result.diagnostics, findings);
    to.manifests.values.push_back(hit.manifest_key);
    to.results.values.push_back(m.results.values[hit.set]);
}

void add_called_unit(types::dependency_record& to, const types::corpus& c,
    const types::descriptor& d, const codec_key& key, std::string_view
    document, const blob_id& manifest, const types::read_log& log,
    const piece& p, const call_rows& rows, const blob_id& registry,
    unsigned threads) {
    const types::record_unit_id unit{static_cast<std::uint32_t>(to.size())};
    open_unit(to, d.name, d.version, key.configuration, document);
    for (std::size_t i = 0; i < log.size(); ++i)
        add_input(to, log.kinds[i], log.keys[i], log.found[i],
            log.values[i]);
    if (log.whole)
        add_input(to, types::input_kind::type_set, {}, 1, key.type_set);
    if (log.registry)
        add_input(to, types::input_kind::type_registry, {}, 1, registry);
    add_record_files(to, p.files, rows.files_begin, rows.files_end,
        threads, p.ids);
    add_record_findings(to, encode_diagnostics(c, p.diagnostics,
        rows.diagnostics_begin, rows.diagnostics_end),
        rows.diagnostics_end - rows.diagnostics_begin);
    to.manifests.values.push_back(manifest);
    to.results.values.push_back(result_key(manifest, to, unit));
}

/**
 * @brief Drops each row of @p files whose path a later row has, as the
 * files of a staged codec are those of the later part.
 */
void keep_last_of_each_path(types::file_table& files) {
    std::unordered_map<std::string_view, std::uint32_t> last;
    last.reserve(files.size());
    for (std::uint32_t f = 0; f < files.size(); ++f)
        last.insert_or_assign(files.paths[types::file_id{f}], f);
    if (last.size() == files.size())
        return;
    types::file_table kept;
    for (std::uint32_t f = 0; f < files.size(); ++f) {
        const types::file_id file{f};
        if (last.find(files.paths[file])->second == f)
            kept.add(files.paths[file], files.contents[file]);
    }
    files = std::move(kept);
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
    const types::action_source& cache, const unit_filter& only,
    std::string_view edited) {
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
    std::vector<std::unique_ptr<staging>> staged(r.size());
    const auto answers_of = [&](types::codec_id codec) {
        const auto& s = staged[codec.value];
        return answers{c, index, flags_of(r, codec), keys[codec.value],
            registry, s ? &s->facts : nullptr, edited};
    };
    const auto prepare = [&](std::uint32_t i, staging& s, types::read_log& log,
        types::stage& out) {
        const auto began = std::chrono::steady_clock::now();
        auto view = views[i];
        log.corpus = true;
        view.record_to(&log);
        s.state = std::get<types::staged_function>(r.codecs[i].run).prepare(
            view, out);
        s.ms += since(began);
    };

    unit_table units;
    std::vector<std::pair<std::uint32_t, cache_hit>> stage_hits;
    const auto add_unit = [&](types::codec_id codec, unit_kind kind,
        std::uint32_t visible, std::uint32_t previous_unit,
        const blob_id& manifest) {
        units.codecs.push_back(codec);
        units.kinds.push_back(kind);
        units.visible.push_back(visible);
        units.previous.push_back(previous_unit);
        units.manifests.push_back(manifest);
        units.seeds.emplace_back();
        units.last.push_back(no_unit);
        units.changed.emplace_back();
    };
    for (std::uint32_t i = 0; i < r.size(); ++i) {
        const types::codec_id codec{i};
        const auto& d = r.codecs[i];
        const auto kind = unit_of(d);
        if (r.enabled[i] != 0)
            rows[i] = visible_rows(r, codec, c);
        views.emplace_back(c, types, index, rows[i], flags_of(r, codec),
            d.options, option_values(r, codec));
        if (r.enabled[i] == 0)
            continue;
        result.documents[i] = rows[i].size();

        if (kind == types::unit::staged) {
            if (only && !only(codec, {}))
                continue;
            keys[i].configuration = configuration_hash(r, codec);
            keys[i].type_set = type_set_hash(c, rows[i]);
            staged[i] = std::make_unique<staging>();
            auto& s = *staged[i];
            const auto now = answers_of(codec);
            const auto manifest = manifest_key(d.name, d.version,
                keys[i].configuration, {}, {});
            const auto published = [&](std::string_view body) {
                const auto decoded = decode_result(body);
                auto stage = decoded ? decode_stage(decoded->scan)
                    : std::nullopt;
                if (stage)
                    s.stage = std::move(*stage);
                return stage.has_value();
            };
            auto previous_unit = no_unit;
            bool known = false;
            const auto it = recorded.find(unit_name(d.name, {}));
            if (it != recorded.end() &&
                reusable(previous, {it->second}, d, now)) {
                // The row stands in whether or not its result object is
                // there to say what it published: with the same inputs,
                // calling prepare publishes the same.
                previous_unit = it->second;
                known = true;
                const auto body = cache ? cache(
                    store::types::action_kind::result,
                    previous.results[{it->second}]) : std::nullopt;
                if (!body || !published(*body)) {
                    types::read_log log;
                    prepare(i, s, log, s.stage);
                }
            }
            if (!known && cache) {
                if (auto hit = look_up(cache, manifest, now);
                    hit && published(hit->body)) {
                    stage_hits.emplace_back(
                        static_cast<std::uint32_t>(units.size()),
                        std::move(*hit));
                    known = true;
                }
            }
            if (!known) {
                prepare(i, s, s.log, s.stage);
                s.called = true;
            }
            s.index();
            add_unit(codec, unit_kind::stage, 0, previous_unit, manifest);

            for (std::uint32_t v = 0; v < s.stage.parts.size(); ++v) {
                const std::string_view part = s.stage.parts[v];
                if (part.empty())
                    throw std::logic_error(std::string(d.name) +
                        " listed a part with no key");
                if (only && !only(codec, part))
                    continue;
                auto stands_in = no_unit;
                auto last = no_unit;
                std::vector<types::changed_input> changed;
                const auto at = recorded.find(unit_name(d.name, part));
                if (at != recorded.end() &&
                    previous.versions[{at->second}] == d.version &&
                    previous.configurations[{at->second}] ==
                        keys[i].configuration) {
                    const auto end = end_of(previous.input_firsts, at->second,
                        previous.input_count());
                    for (auto in = previous.input_firsts[{at->second}].value;
                         in < end; ++in) {
                        if (!now.holds(previous, types::input_id{in}))
                            changed.push_back({previous.input_kinds[{in}],
                                previous.input_keys[{in}]});
                    }
                    (changed.empty() ? stands_in : last) = at->second;
                }
                add_unit(codec, unit_kind::part, v, stands_in,
                    manifest_key(d.name, d.version, keys[i].configuration,
                        part, {}));
                units.last.back() = last;
                units.changed.back() = std::move(changed);
            }
            continue;
        }

        const auto corpus_unit = kind == types::unit::corpus;
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
            add_unit(codec, corpus_unit ? unit_kind::corpus
                : unit_kind::document, v, previous_unit,
                manifest_key(d.name, d.version, keys[i].configuration,
                    document, subject));
            if (std::holds_alternative<types::scan_function>(d.run) &&
                it != recorded.end() &&
                previous.versions[{it->second}] == d.version &&
                previous.configurations[{it->second}] ==
                    keys[i].configuration) {
                units.seeds.back() = previous.results[{it->second}];
                units.last.back() = it->second;
            }
        }
    }
    const auto document_of = [&](std::uint32_t u) -> std::string_view {
        const auto codec = units.codecs[u].value;
        switch (units.kinds[u]) {
        case unit_kind::document:
            return c.paths[rows[codec][units.visible[u]].value];
        case unit_kind::part:
            return staged[codec]->stage.parts[units.visible[u]];
        case unit_kind::corpus:
        case unit_kind::stage:
            break;
        }
        return {};
    };

    const auto lookup_start = std::chrono::steady_clock::now();
    std::vector<std::optional<cache_hit>> hits(units.size());
    for (auto& [unit, hit] : stage_hits)
        hits[unit] = std::move(hit);
    if (cache) {
        std::vector<std::uint32_t> unknown;
        for (std::uint32_t u = 0; u < units.size(); ++u) {
            if (units.previous[u] == no_unit &&
                units.kinds[u] != unit_kind::stage && !units.seeds[u])
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
            if (units.previous[u] == no_unit && !hits[u] &&
                units.kinds[u] != unit_kind::stage)
                called.push_back(u);
        }
        const auto count = called.size() - first;
        if (count == 0)
            continue;
        if (auto& s = staged[codec.value]; s && !s->state) {
            // The record or the cache stood in for prepare, so it is
            // called for the state its parts run from.
            types::read_log log;
            types::stage again;
            prepare(codec.value, *s, log, again);
        }
        const auto parts = unit_of(r.codecs[codec.value]) ==
            types::unit::corpus ? 1 : std::min(count,
                threads * pieces_per_thread);
        for (std::size_t i = 0; i < parts; ++i)
            pieces.push_back({codec, first + count * i / parts,
                first + count * (i + 1) / parts, {}, {}, 0, {}, {}});
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
            scanning, scans, previous, staged[p.codec.value].get(),
            answers_of(p.codec)}, r.codecs[p.codec.value].run);
        p.ms = since(began);
    });

    for (const auto& p : pieces)
        result.ms[p.codec.value] += p.ms;
    for (std::size_t i = 0; i < staged.size(); ++i) {
        if (staged[i])
            result.ms[i] += staged[i]->ms;
    }
    const piece nothing{};
    for (std::uint32_t u = 0; u < units.size(); ++u) {
        const auto codec = units.codecs[u];
        const auto& d = r.codecs[codec.value];
        auto& files = result.files[codec.value];
        auto& diagnostics = result.diagnostics[codec.value];
        const auto document = document_of(u);
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
                    before), threads);
            ++result.cached[codec.value];
            continue;
        }
        result.fresh.push_back(
            {static_cast<std::uint32_t>(result.record.size())});
        ++result.ran[codec.value];
        if (units.kinds[u] == unit_kind::stage) {
            const auto& s = *staged[codec.value];
            result.scans.push_back(encode_stage(s.stage));
            add_called_unit(result.record, c, d, keys[codec.value], document,
                units.manifests[u], s.log, nothing, {}, registry, threads);
            continue;
        }
        const auto& at = call_outputs[u];
        const auto& p = pieces[at.piece];
        add_files(files, p.files, at.files_begin, at.files_end);
        add_diagnostics(diagnostics, p.diagnostics, at.diagnostics_begin,
            at.diagnostics_end);
        result.scans.push_back(std::move(scans[u]));
        add_called_unit(result.record, c, d, keys[codec.value], document,
            units.manifests[u], logs[u], p, at, registry, threads);
    }
    for (std::size_t i = 0; i < staged.size(); ++i) {
        if (staged[i])
            keep_last_of_each_path(result.files[i]);
    }
    const auto skipped = std::ranges::fold_left(result.skipped,
        std::uint64_t{0}, std::plus<>{});
    result.same_record = skipped == units.size() &&
        previous.size() == units.size();
    result.wall_ms = since(start);
    return result;
}

}

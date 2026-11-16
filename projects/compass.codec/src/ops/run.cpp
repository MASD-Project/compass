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
#include <thread>
#include <tuple>
#include <variant>
#include <vector>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.org/ops/type_lookup.hpp"

namespace compass::codec::ops {

namespace {

namespace ot = compass::org::types;

/**
 * @brief How many pieces a document codec is split into per thread, so a
 * thread that drew cheap documents takes more of them.
 */
constexpr std::size_t pieces_per_thread = 4;

/**
 * @brief One call of a corpus codec, or the calls of a document codec for
 * the visible documents [begin, end), with the tables they fill.
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

struct call {
    piece& p;
    const types::corpus_view& view;

    template <typename Output>
    void operator()(types::document_function<Output> f) const {
        auto& out = output<Output>(p);
        for (auto i = p.begin; i < p.end; ++i)
            f(view[i], view, out);
    }

    template <typename Output>
    void operator()(types::corpus_function<Output> f) const {
        f(view, output<Output>(p));
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

void add_pieces(std::vector<piece>& pieces, types::codec_id codec,
    types::unit unit, std::size_t documents, unsigned threads) {
    if (unit == types::unit::corpus) {
        pieces.push_back({codec, 0, documents, {}, {}, 0});
        return;
    }
    const auto count = std::min(documents, threads * pieces_per_thread);
    for (std::size_t i = 0; i < count; ++i)
        pieces.push_back({codec, documents * i / count,
            documents * (i + 1) / count, {}, {}, 0});
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
    const ot::type_registry& types, unsigned threads) {
    const auto start = std::chrono::steady_clock::now();
    threads = std::max(threads, 1u);

    types::run_result result;
    result.documents.assign(r.size(), 0);
    result.files.resize(r.size());
    result.diagnostics.resize(r.size());
    result.ms.assign(r.size(), 0);

    std::vector<std::vector<ot::document_id>> rows(r.size());
    std::vector<types::corpus_view> views;
    views.reserve(r.size());
    std::vector<piece> pieces;
    for (std::uint32_t i = 0; i < r.size(); ++i) {
        const types::codec_id codec{i};
        if (r.enabled[i] != 0)
            rows[i] = visible_rows(r, codec, c);
        views.emplace_back(c, types, rows[i], r.codecs[i].options,
            option_values(r, codec));
        if (r.enabled[i] == 0)
            continue;
        result.documents[i] = rows[i].size();
        add_pieces(pieces, codec, unit_of(r.codecs[i]), rows[i].size(),
            threads);
    }

    std::atomic<std::size_t> next{0};
    std::mutex failure_lock;
    std::exception_ptr failure;
    const auto work = [&] {
        for (auto i = next++; i < pieces.size(); i = next++) {
            auto& p = pieces[i];
            const auto began = std::chrono::steady_clock::now();
            try {
                std::visit(call{p, views[p.codec.value]},
                    r.codecs[p.codec.value].run);
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

    for (const auto& p : pieces) {
        result.files[p.codec.value].append(p.files);
        result.diagnostics[p.codec.value].append(p.diagnostics);
        result.ms[p.codec.value] += p.ms;
    }
    result.wall_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    return result;
}

}

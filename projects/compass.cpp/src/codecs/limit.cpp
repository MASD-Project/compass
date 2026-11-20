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
#include "compass.cpp/codecs/limit.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/limit.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;

/**
 * @brief The definition @p text states, parsed once per thread, or null
 * when it does not parse.
 */
const ops::limit_definition* definition_of(std::string_view text) {
    thread_local std::unordered_map<std::string,
        std::optional<ops::limit_definition>> cache;
    auto it = cache.find(std::string(text));
    if (it == cache.end()) {
        auto parsed = ops::parse_limit_definition(text);
        it = cache.emplace(std::string(text), parsed
            ? std::optional(std::move(*parsed)) : std::nullopt).first;
    }
    return it->second ? &*it->second : nullptr;
}

std::vector<std::string_view> files(const ct::corpus_view& v) {
    const auto* d = definition_of(v.option(limit_definition_key));
    return d == nullptr ? std::vector<std::string_view>{}
        : ops::covered_files(v, d->files, d->outside);
}

void map(std::string_view, std::string_view bytes,
    const ct::option_values& options, ct::file_scan& out) {
    if (const auto* d = definition_of(options[limit_definition_key]))
        out.fact = ops::limit_fact(*d, bytes);
}

void reduce(std::span<const ct::scanned_file> scanned,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    if (const auto* d = definition_of(v.option(limit_definition_key)))
        ops::limit_findings(*d, scanned, v.read_file(d->source).value_or(""),
            out);
}

}

ct::scan_function limit_function() noexcept {
    return {files, map, reduce};
}

}

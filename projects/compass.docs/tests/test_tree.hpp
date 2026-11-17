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
#ifndef COMPASS_DOCS_TESTS_TEST_TREE_HPP
#define COMPASS_DOCS_TESTS_TEST_TREE_HPP

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/corpus.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::docs::tests {

using files = std::map<std::string, std::string, std::less<>>;
using options = std::vector<std::pair<std::string, std::string>>;

/**
 * @brief A finding as path, 1-based line and message.
 */
using finding = std::tuple<std::string, std::uint32_t, std::string>;

/**
 * @brief Runs @p codec, enabled with @p settings, over a checkout of
 * @p all, every .org file of it parsed into the corpus with no type.
 */
/**
 * @brief Runs @p codec alone over @p all, the .org files as documents and
 * every file as the checkout, and hands the result and the corpus to
 * @p consume.
 */
template <typename Consume>
auto with_run(const compass::codec::types::descriptor& codec,
    const files& all, const options& settings, Consume consume) {
    namespace ct = compass::codec::types;
    namespace ot = compass::org::types;
    ct::checkout_files checkout;
    for (const auto& [path, bytes] : all)
        checkout.paths.push_back(path);
    checkout.content_of = [&](std::string_view p) {
        return std::optional(store::ops::blob_id_of(all.find(p)->second));
    };
    checkout.read = [&](std::string_view p) {
        return std::optional(all.find(p)->second);
    };
    std::deque<ot::tables> tables;
    ct::corpus corpus;
    for (const auto& [path, bytes] : all) {
        if (!path.ends_with(".org"))
            continue;
        auto& t = tables.emplace_back();
        ingest::org::parse(bytes, t);
        corpus.paths.emplace_back(path);
        corpus.contents.push_back(store::ops::blob_id_of(bytes));
        corpus.sources.emplace_back(bytes);
        corpus.tables.push_back(ot::view(t));
        corpus.types.push_back(ot::type_id::none());
    }
    corpus.files = &checkout;
    const ot::type_registry types;
    ct::configuration configuration;
    configuration.set(codec.name, "enabled", "true");
    for (const auto& [key, value] : settings)
        configuration.set(codec.name, key, value);
    const std::array<ct::descriptor, 1> codecs{codec};
    const auto registry = compass::codec::ops::make_registry(codecs, types,
        configuration);
    return consume(compass::codec::ops::run(registry, corpus, types, 2),
        corpus);
}

inline std::vector<finding> run(
    const compass::codec::types::descriptor& codec, const files& all,
    const options& settings = {}) {
    namespace ct = compass::codec::types;
    return with_run(codec, all, settings, [&](const ct::run_result& r,
        const ct::corpus& corpus) {
        std::vector<finding> out;
        const auto& t = r.diagnostics[0];
        for (std::uint32_t i = 0; i < t.size(); ++i) {
            const auto d = t[ct::diagnostic_id{i}];
            const std::string path = d.document.is_none()
                ? std::string(d.file)
                : std::string(corpus.paths[d.document.value]);
            const auto file = all.find(path);
            out.emplace_back(path, file == all.end() ? 0
                    : compass::cpp::ops::line_of(file->second, d.range.begin),
                std::string(d.message));
        }
        return out;
    });
}

/**
 * @brief The files @p codec writes over @p all, by path.
 */
inline files generate(const compass::codec::types::descriptor& codec,
    const files& all, const options& settings = {}) {
    namespace ct = compass::codec::types;
    return with_run(codec, all, settings, [](const ct::run_result& r,
        const ct::corpus&) {
        files out;
        const auto& f = r.files[0];
        for (std::uint32_t i = 0; i < f.size(); ++i)
            out.emplace(f.paths[ct::file_id{i}], f.contents[ct::file_id{i}]);
        return out;
    });
}

/**
 * @brief The path and line of each of @p findings.
 */
inline std::vector<std::pair<std::string, std::uint32_t>> places(
    const std::vector<finding>& findings) {
    std::vector<std::pair<std::string, std::uint32_t>> out;
    for (const auto& [path, line, message] : findings)
        out.emplace_back(path, line);
    return out;
}

/**
 * @brief The message of each of @p findings.
 */
inline std::vector<std::string> messages(const std::vector<finding>& findings) {
    std::vector<std::string> out;
    for (const auto& [path, line, message] : findings)
        out.push_back(message);
    return out;
}

}

#endif

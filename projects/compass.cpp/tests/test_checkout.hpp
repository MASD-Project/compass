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
#ifndef COMPASS_CPP_TESTS_TEST_CHECKOUT_HPP
#define COMPASS_CPP_TESTS_TEST_CHECKOUT_HPP

#include <array>
#include <deque>
#include <filesystem>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/io/action_cache.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/corpus.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.testing/filesystem/scoped_directory.hpp"

namespace compass::cpp::tests {

using files = std::map<std::string, std::string, std::less<>>;

/**
 * @brief A finding as path, 1-based line and message.
 */
using finding = std::tuple<std::string, std::uint32_t, std::string>;

/**
 * @brief What one run of a codec over a checkout found, and how many of
 * its units ran.
 */
struct outcome {
    std::vector<finding> findings;
    /** The fix of each finding, in the same order. */
    std::vector<std::string> fixes;
    std::uint64_t ran = 0;
    compass::codec::types::dependency_record record;
};

/**
 * @brief Runs @p codec, enabled, over a checkout of @p all, its .org
 * files parsed into untyped documents, from @p previous and, when @p cache is not empty, the
 * result cache under it, where it then stores what ran.
 */
inline outcome run(const compass::codec::types::descriptor& codec,
    const files& all,
    const compass::codec::types::dependency_record& previous = {},
    const std::filesystem::path& cache = {}) {
    namespace ct = compass::codec::types;
    ct::checkout_files checkout;
    for (const auto& [path, bytes] : all)
        checkout.paths.push_back(path);
    checkout.content_of = [&](std::string_view p) {
        return std::optional(store::ops::blob_id_of(all.find(p)->second));
    };
    checkout.read = [&](std::string_view p) {
        return std::optional(all.find(p)->second);
    };
    namespace ot = compass::org::types;
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
    const compass::org::types::type_registry types;
    ct::configuration settings;
    settings.set(codec.name, "enabled", "true");
    const std::array<ct::descriptor, 1> codecs{codec};
    const auto registry = compass::codec::ops::make_registry(codecs, types,
        settings);
    auto r = compass::codec::ops::run(registry, corpus, types, 1, previous,
        cache.empty() ? ct::action_source{}
                      : compass::codec::io::action_source_of(cache));
    if (!cache.empty())
        (void)compass::codec::io::store_actions(cache, r.record, r.fresh, 1,
            r.scans, r.scan_facts);
    outcome out;
    const auto& t = r.diagnostics[0];
    for (std::uint32_t i = 0; i < t.size(); ++i) {
        const auto d = t[ct::diagnostic_id{i}];
        const auto file = all.find(d.file);
        out.findings.emplace_back(std::string(d.file), file == all.end()
                ? 0 : ops::line_of(file->second, d.range.begin),
            std::string(d.message));
        out.fixes.emplace_back(d.fix);
    }
    out.ran = r.ran[0];
    out.record = std::move(r.record);
    return out;
}

/**
 * @brief Runs @p codec over @p before, then over @p after from that run's
 * record and result cache, as a check after an edit does, and checks that
 * it finds what a run over @p after alone finds. Returns the second run.
 */
inline outcome run_after_edit(const compass::codec::types::descriptor& codec,
    const files& before, const files& after) {
    const compass::testing::filesystem::scoped_directory cache(
        "compass.cpp.tests.edit");
    const auto first = run(codec, before, {}, cache.path());
    auto second = run(codec, after, first.record, cache.path());
    CHECK(second.findings == run(codec, after).findings);
    return second;
}

/**
 * @brief The files the file codec @p codec, enabled, writes over a checkout
 * of @p all, by path.
 */
inline files generate(const compass::codec::types::descriptor& codec,
    const files& all) {
    namespace ct = compass::codec::types;
    ct::checkout_files checkout;
    for (const auto& [path, bytes] : all)
        checkout.paths.push_back(path);
    checkout.content_of = [&](std::string_view p) {
        return std::optional(store::ops::blob_id_of(all.find(p)->second));
    };
    checkout.read = [&](std::string_view p) {
        return std::optional(all.find(p)->second);
    };
    ct::corpus corpus;
    corpus.files = &checkout;
    const compass::org::types::type_registry types;
    ct::configuration settings;
    settings.set(codec.name, "enabled", "true");
    const std::array<ct::descriptor, 1> codecs{codec};
    const auto r = compass::codec::ops::run(compass::codec::ops::make_registry(
        codecs, types, settings), corpus, types, 1);
    files out;
    const auto& f = r.files[0];
    for (std::uint32_t i = 0; i < f.size(); ++i)
        out.emplace(f.paths[ct::file_id{i}], f.contents[ct::file_id{i}]);
    return out;
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

}

#endif

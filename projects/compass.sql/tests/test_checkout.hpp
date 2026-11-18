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
#ifndef COMPASS_SQL_TESTS_TEST_CHECKOUT_HPP
#define COMPASS_SQL_TESTS_TEST_CHECKOUT_HPP

#include <algorithm>
#include <array>
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
#include "compass.platform/process/environment.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::sql::tests {

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
    std::uint64_t ran = 0;
    compass::codec::types::dependency_record record;
};

/**
 * @brief Runs @p codec, enabled, over a checkout of @p all and no
 * documents, from @p previous and, when @p cache is not empty, the
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
    ct::corpus corpus;
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
            r.scans);
    outcome out;
    const auto& t = r.diagnostics[0];
    for (std::uint32_t i = 0; i < t.size(); ++i) {
        const auto d = t[ct::diagnostic_id{i}];
        const auto file = all.find(d.file);
        std::uint32_t line = 0;
        if (file != all.end())
            line = static_cast<std::uint32_t>(std::count(
                file->second.begin(), file->second.begin() + d.range.begin,
                '\n') + 1);
        out.findings.emplace_back(std::string(d.file), line,
            std::string(d.message));
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
    static int counter = 0;
    const auto cache = std::filesystem::temp_directory_path() /
        ("compass.tests.edit." +
            std::to_string(
                compass::platform::process::current_process_id()) +
            "." + std::to_string(counter++));
    std::filesystem::create_directories(cache);
    const auto first = run(codec, before, {}, cache);
    auto second = run(codec, after, first.record, cache);
    std::filesystem::remove_all(cache);
    CHECK(second.findings == run(codec, after).findings);
    return second;
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

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
#ifndef COMPASS_CROSSCHECK_TESTS_TEST_CHECKOUT_HPP
#define COMPASS_CROSSCHECK_TESTS_TEST_CHECKOUT_HPP

#include <array>
#include <cstdint>
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
#include "compass.store/ops/blob_hash.hpp"

namespace compass::crosscheck::tests {

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

using options = std::vector<std::pair<std::string, std::string>>;

/**
 * @brief Runs @p codec, enabled with @p settings, over a checkout of
 * @p all and no documents, from @p previous.
 */
inline outcome run(const compass::codec::types::descriptor& codec,
    const files& all, const options& settings_of = {},
    const compass::codec::types::dependency_record& previous = {}) {
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
    for (const auto& [key, value] : settings_of)
        settings.set(codec.name, key, value);
    const std::array<ct::descriptor, 1> codecs{codec};
    const auto registry = compass::codec::ops::make_registry(codecs, types,
        settings);
    auto r = compass::codec::ops::run(registry, corpus, types, 1, previous);
    outcome out;
    const auto& t = r.diagnostics[0];
    for (std::uint32_t i = 0; i < t.size(); ++i) {
        const auto d = t[ct::diagnostic_id{i}];
        const auto file = all.find(d.file);
        out.findings.emplace_back(std::string(d.file), file == all.end()
                ? 0 : compass::cpp::ops::line_of(file->second, d.range.begin),
            std::string(d.message));
    }
    out.ran = r.ran[0];
    out.record = std::move(r.record);
    return out;
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
 * @brief The message of each of @p findings.
 */
inline std::vector<std::string> messages_of(const outcome& r) {
    std::vector<std::string> out;
    for (const auto& [path, line, message] : r.findings)
        out.push_back(message);
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

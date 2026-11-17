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
#ifndef COMPASS_CODEC_OPS_RULE_TABLE_HPP
#define COMPASS_CODEC_OPS_RULE_TABLE_HPP

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/descriptor.hpp"

namespace compass::codec::ops {

/**
 * @brief One row of the rule table: the codec that reports the rule, or
 * the runtime for the rules no descriptor lists, and the rule.
 */
struct rule_row {
    std::string_view codec;
    const types::rule* rule = nullptr;
};

/**
 * @brief The codec cell of the drift row, which every files codec shares.
 */
inline constexpr std::string_view every_files_codec = "/each files codec/";

/**
 * @brief Every rule of @p catalogue: the rules each codec declares,
 * ordered by codec name, each codec's in the order it declares them, then
 * the exception rules, then drift once for every codec that emits files.
 */
[[nodiscard]] std::vector<rule_row> rules_of(
    std::span<const types::descriptor> catalogue);

/**
 * @brief A number that changes whenever the rule table of @p catalogue
 * would: a hash of the format of the page and of the codec, name,
 * statement and citations of each row of rules_of.
 */
[[nodiscard]] std::uint32_t rule_table_version(
    std::span<const types::descriptor> catalogue);

inline constexpr std::array<types::option, 1> rule_table_options{{
    {"path", "doc/knowledge/compass/rule_table.org",
        "Where the page goes, relative to the root of the checkout."}}};

/**
 * @brief The org page of the rules of @p catalogue and the memories of
 * @p documents.
 *
 * One table row per rule of rules_of: its codec, its name, its statement
 * and a link to each memory or document it cites, titled when @p documents
 * holds it and as the bare ID when it does not. Then every document of
 * type memory that no rule cites, by path.
 */
[[nodiscard]] std::string rule_table(
    std::span<const types::descriptor> catalogue,
    const types::corpus_view& documents);

}

#endif

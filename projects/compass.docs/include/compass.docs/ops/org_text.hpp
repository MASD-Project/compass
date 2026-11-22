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
#ifndef COMPASS_DOCS_OPS_ORG_TEXT_HPP
#define COMPASS_DOCS_OPS_ORG_TEXT_HPP

#include <cstdint>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"

namespace compass::docs::ops {

/**
 * @brief The trimmed value of the first keyword of @p d named @p key, or
 * empty when it has none. @p key is in lower case: "title" finds #+TITLE:.
 */
[[nodiscard]] std::string_view keyword_value(
    const compass::codec::types::document& d, std::string_view key);

/**
 * @brief The text of @p s up to its first space.
 */
[[nodiscard]] std::string_view first_word(std::string_view s) noexcept;

/**
 * @brief The byte at which the line holding byte @p offset of @p source
 * starts.
 */
[[nodiscard]] std::uint32_t line_start(std::string_view source,
    std::uint32_t offset) noexcept;

/**
 * @brief A comment element: the byte it starts at and its lines, each a
 * line of the source that starts with "#".
 */
struct comment {
    std::uint32_t begin = 0;
    std::vector<std::string_view> lines;
};

/**
 * @brief The comment elements of @p d, in document order.
 */
[[nodiscard]] std::vector<comment> comments(
    const compass::codec::types::document& d);

/**
 * @brief A bracket link: the byte it starts at, its path as written,
 * trimmed, and its description, empty when it has none.
 */
struct bracket_link {
    std::uint32_t begin = 0;
    std::string_view target;
    std::string_view label;
};

/**
 * @brief The bracket links of @p d whose type is @p type, or of any type
 * when @p type is empty, in source order.
 */
[[nodiscard]] std::vector<bracket_link> bracket_links(
    const compass::codec::types::document& d, std::string_view type = {});

}

#endif

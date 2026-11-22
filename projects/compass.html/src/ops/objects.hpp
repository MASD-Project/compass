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
#ifndef COMPASS_HTML_SRC_OPS_OBJECTS_HPP
#define COMPASS_HTML_SRC_OPS_OBJECTS_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include "compass.org/types/tables.hpp"

namespace compass::html::ops::detail {

/**
 * @brief The object rows an element holds directly: [first, last).
 */
struct object_run {
    org::types::object_id first;
    org::types::object_id last;
};

[[nodiscard]] object_run own_objects(const org::types::tables_view& t,
    org::types::element_id e) noexcept;

/**
 * @brief The objects nested in @p o.
 */
[[nodiscard]] object_run nested_objects(const org::types::tables_view& t,
    org::types::object_id o) noexcept;

/**
 * @brief The range of @p o without the blanks after it.
 */
[[nodiscard]] org::types::source_range without_post_blanks(
    std::string_view source, const org::types::tables_view& t,
    org::types::object_id o) noexcept;

/**
 * @brief The link row of the link object @p o.
 */
[[nodiscard]] org::types::link_id link_of(const org::types::tables_view& t,
    org::types::object_id o) noexcept;

/**
 * @brief The path of a link, unescaped.
 */
[[nodiscard]] std::string link_path(std::string_view source,
    const org::types::tables_view& t, org::types::link_id l);

/**
 * @brief @p raw with the backslashes that escape brackets removed and each
 * line break, with the blanks after it, made one space.
 */
[[nodiscard]] std::string unescaped(std::string_view raw);

/**
 * @brief The text a link with no description shows: what is written
 * inside its brackets, or the link itself when it has none.
 */
[[nodiscard]] std::string link_text(std::string_view source,
    const org::types::tables_view& t, org::types::object_id o);

/**
 * @brief Appends the text a reader sees in @p contents, whose objects are
 * @p run: markup without its markers, a link by its description or else
 * its link text.
 */
void append_plain_text(std::string& out, std::string_view source,
    const org::types::tables_view& t, org::types::source_range contents,
    object_run run);

/**
 * @brief The value of the property @p key of heading @p h, trimmed, or
 * empty. @p key is in upper case.
 */
[[nodiscard]] std::string_view heading_property(std::string_view source,
    const org::types::tables_view& t, org::types::heading_id h,
    std::string_view key);

}

#endif

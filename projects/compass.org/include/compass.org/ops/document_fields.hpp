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
#ifndef COMPASS_ORG_OPS_DOCUMENT_FIELDS_HPP
#define COMPASS_ORG_OPS_DOCUMENT_FIELDS_HPP

#include <string_view>
#include "compass.org/types/handle.hpp"
#include "compass.org/types/source_range.hpp"
#include "compass.org/types/tables.hpp"

namespace compass::org::ops {

/**
 * @brief The text of an interned name of one document.
 */
[[nodiscard]] std::string_view name_text(const types::tables_view& t,
    types::name_id n) noexcept;

/**
 * @brief The first keyword of the document named @p key, or none.
 *
 * @p key is in lower case, as the keyword table holds it: "title" finds
 * #+title: and #+TITLE:.
 */
[[nodiscard]] types::keyword_id find_keyword(const types::tables_view& t,
    std::string_view key) noexcept;

/**
 * @brief The first property of the document itself named @p key, or none.
 *
 * The document's properties are those of the drawer before its first
 * heading. @p key is in upper case, as the property table holds it.
 */
[[nodiscard]] types::property_id find_document_property(
    const types::tables_view& t, std::string_view key) noexcept;

/**
 * @brief @p range without the spaces, tabs and line ends at either end.
 */
[[nodiscard]] types::source_range trimmed(std::string_view source,
    types::source_range range) noexcept;

}

#endif

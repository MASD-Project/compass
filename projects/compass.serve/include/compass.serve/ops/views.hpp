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
#ifndef COMPASS_SERVE_OPS_VIEWS_HPP
#define COMPASS_SERVE_OPS_VIEWS_HPP

#include <expected>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.db/types/search.hpp"
#include "compass.serve/types/settings.hpp"

namespace compass::serve::ops {

/**
 * @brief The views the design names, for a project that declares none:
 * team sees every document, knowledge the knowledge and manual pages, and
 * public the pages for users.
 */
[[nodiscard]] std::vector<types::view_definition> default_views();

/**
 * @brief The SQL condition over corpus.document d that @p documents states.
 *
 * @p documents is "all", or conditions separated by ";", each a field, a
 * colon and the values it may take separated by spaces: "type: knowledge
 * manual; audience: user". The fields are type, level, audience and state,
 * which hold one of the values, and tag, where the document carries any of
 * them. Every value is quoted as an SQL literal, so a view's text never runs
 * as SQL.
 * @returns the condition, or why @p documents is not one.
 */
[[nodiscard]] std::expected<std::string, std::string> compile_view(std::string_view documents);

/**
 * @brief Each view of @p views by name, compiled.
 * @returns the views, or the first that does not compile or repeats a name.
 */
[[nodiscard]] std::expected<std::map<std::string, db::types::view_filter, std::less<>>,
                            std::string>
compile_views(std::span<const types::view_definition> views);

}

#endif

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
#ifndef COMPASS_CLI_APP_CHECK_INVENTORY_HPP
#define COMPASS_CLI_APP_CHECK_INVENTORY_HPP

#include <span>
#include <string>
#include <string_view>
#include "compass.cli/config/check_documents.hpp"
#include "compass.codec/types/configuration.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/descriptor.hpp"

namespace compass::cli::app {

/**
 * @brief The key of check_inventory's option that holds the inventory,
 * which compass sets from the configuration, not the user.
 */
inline constexpr std::string_view inventory_option = "inventory";

/**
 * @brief The codecs a run sees, as data tables for check_inventory: the
 * table Codecs, a row per built-in codec of @p built_in and per check of
 * @p checks, with whether @p settings enable it, its kind, severity and
 * summary, and the path and ID of the document that defines it or gives
 * it data; and the table Rules, a row per rule.
 */
[[nodiscard]] std::string inventory_of(
    std::span<const codec::types::descriptor> built_in,
    const config::check_set& checks,
    const codec::types::configuration& settings);

/**
 * @brief The inventory page check_inventory writes from its options: the
 * frontmatter they state, a summary table of every codec, enabled first,
 * then a section per codec with its rules and where it is defined.
 */
[[nodiscard]] std::string inventory_page(const codec::types::corpus_view& v);

}

#endif

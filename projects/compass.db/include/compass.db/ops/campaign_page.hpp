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
#ifndef COMPASS_DB_OPS_CAMPAIGN_PAGE_HPP
#define COMPASS_DB_OPS_CAMPAIGN_PAGE_HPP

#include <string>
#include <string_view>
#include <vector>
#include "compass.db/types/campaign_export.hpp"

namespace compass::db::ops {

/**
 * @brief The :ID: of the test_campaign contract page, Document type:
 * test_campaign, which every campaign page links.
 */
inline constexpr std::string_view test_campaign_contract_id =
    "42967863-F931-4D04-AF82-2CF47C9758FE";

/**
 * @brief The :ID: an exported record gets from @p name: the SHA-1 of the
 * name laid out as a version 5 UUID, in upper case. The same name always
 * gives the same ID.
 */
[[nodiscard]] std::string derived_id(std::string_view name);

/**
 * @brief The files of campaign @p c, a pure function of its rows: the
 * test_campaign page, test_campaign_<id>.org in its story's folder, and
 * each screenshot it shows, in the folder test_campaign_<id> beside the
 * page, named by its blob ID with its media type's extension. The page
 * comes first. Paths are relative to the environment's root, as
 * c.story_path is, which must not be empty.
 */
[[nodiscard]] std::vector<types::exported_file> render_campaign(
    const types::campaign_rows& c);

}

#endif

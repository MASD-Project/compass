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
#include "compass.cli/app/test_export.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include "compass.db/ops/campaign_page.hpp"
#include "compass.db/query/campaign_export.hpp"
#include "compass.platform/filesystem/new_file.hpp"

namespace compass::cli::app {

namespace {

/**
 * @brief @p relative under @p checkout.
 * @throws std::runtime_error when @p relative is absolute or climbs out of
 * the checkout.
 */
std::filesystem::path inside(const std::filesystem::path& checkout,
    const std::string& relative) {
    const auto path = std::filesystem::path(relative).lexically_normal();
    if (path.has_root_path() || path.empty() || *path.begin() == "..")
        throw std::runtime_error("refusing to write " + relative +
            ", which is outside the checkout");
    return checkout / path;
}

}

export_report export_campaigns(db::io::connection& c,
    const std::filesystem::path& checkout, const std::string& environment,
    const std::optional<std::string>& story) {
    export_report out;
    std::vector<std::int64_t> exported;
    const auto snapshot = db::query::read_campaigns(c, environment, story);
    for (const auto& campaign : snapshot.campaigns) {
        if (campaign.story_path.empty()) {
            out.skipped.push_back(campaign.story_id);
            continue;
        }
        for (const auto& image : campaign.images)
            out.not_png += image.media_type == "image/png" ? 0 : 1;
        const auto files = db::ops::render_campaign(campaign);
        std::vector<std::filesystem::path> paths;
        for (const auto& f : files)
            paths.push_back(inside(checkout, f.path));
        for (std::size_t i = 0; i < files.size(); ++i) {
            const auto& f = files[i];
            if (platform::filesystem::update_file(paths[i], f.bytes))
                out.written.push_back(f.path);
            else
                ++out.unchanged;
        }
        exported.push_back(campaign.id);
    }
    db::query::mark_exported(c, exported, snapshot.taken_at);
    out.campaigns = exported.size();
    return out;
}

}

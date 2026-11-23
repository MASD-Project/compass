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
/**
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Template: masd_cpp_row.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_DB_TYPES_DOCUMENT_ROW_HPP
#define COMPASS_DB_TYPES_DOCUMENT_ROW_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace compass::db::types {

/**
 * @brief One org document of the corpus.
 *
 * A row of corpus.document.
 */
struct document_row {
    std::optional<std::string> org_id;
    std::optional<std::string> title;
    std::optional<std::string> description;
    std::optional<std::string> type;
    std::optional<std::string> level;
    std::optional<std::string> audience;
    std::optional<std::string> created;
    std::optional<std::string> updated;
    std::vector<std::string> tags;
    std::string keywords;
    std::string properties;
    std::string_view source;
};

}

#endif

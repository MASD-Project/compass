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
#ifndef COMPASS_SERVE_TYPES_SETTINGS_HPP
#define COMPASS_SERVE_TYPES_SETTINGS_HPP

#include <optional>
#include <string>
#include <vector>
#include "compass.html/types/render_config.hpp"

namespace compass::serve::types {

/**
 * @brief A view as the project's configuration declares it: its name and
 * which documents it holds, all or conditions on their metadata.
 */
struct view_definition {
    std::string name;
    std::string documents;
};

/**
 * @brief How compass serve answers: the views it serves, the URLs the HTML
 * gives documents, the secret a gateway must send when one is set, and the
 * caller a request without the gateway's headers takes when serving
 * locally.
 */
struct settings {
    std::vector<view_definition> views;
    html::types::render_config render;
    std::string gateway_secret;
    std::optional<std::string> local_view;
    std::string local_subject;
    std::string version;
};

}

#endif

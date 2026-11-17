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
#ifndef COMPASS_DOCS_CODECS_PROTOTYPES_HPP
#define COMPASS_DOCS_CODECS_PROTOTYPES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's prototypes gate (check_prototype_inventory.py): the
 * prototype inventory matches the tree.
 *
 * A prototype is doc/prototypes/<group>/<name>/index.html, a group page
 * doc/prototypes/<group>/index.org and the top page doc/prototypes/index.org.
 * Rules: unlisted, missing, no_group_page, unlinked_group and missing_group
 * relate the three; review_without_index and review_cell relate a
 * prototype's review/ directory to its row; no_discovery needs the site
 * build to call ores-deploy-prototypes; copy_differs holds the files the
 * trades and conventions groups share by copy identical; react_tree reports
 * the retired React prototype directories.
 */
extern const compass::codec::types::descriptor prototypes;

}

#endif

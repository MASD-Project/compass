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
#ifndef COMPASS_DOCS_CODECS_LINT_HPP
#define COMPASS_DOCS_CODECS_LINT_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's lint gate (compass lint): the filetags, generated
 * markers and id links of the org pages.
 *
 * Reads every .org file of the checkout but those under build, venv,
 * .packages, vcpkg or external at the root, those under a .git, .claude,
 * node_modules or __pycache__ directory, and those whose name starts with
 * a dot. Rules: bad_filetag, unclosed_marker, marker_without_writer,
 * malformed_link, dangling_link, and durable_link, which the gate only
 * advises on and the codec reports only with strict_links=true.
 *
 * A scan: a page's IDs, type, links and markers are its fact, so an edit
 * to one page maps that page alone.
 */
extern const compass::codec::types::descriptor lint;

}

#endif

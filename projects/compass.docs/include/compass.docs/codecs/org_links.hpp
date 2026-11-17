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
#ifndef COMPASS_DOCS_CODECS_ORG_LINKS_HPP
#define COMPASS_DOCS_CODECS_ORG_LINKS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's org-links gate: no link's raw target begins with a
 * colon.
 *
 * Reads every document whose path holds no .packages, vcpkg, build or tmp
 * directory and no .claude/worktrees, from its parsed links. Rule
 * colon_link: a bracket link whose text after the brackets begins with a
 * colon, the shape a verbatim span closed early leaves, as in
 * [[:space:]]. The script parses only files that hold "[[:"; the parse is
 * in the corpus already, so every document is read.
 */
extern const compass::codec::types::descriptor org_links;

}

#endif

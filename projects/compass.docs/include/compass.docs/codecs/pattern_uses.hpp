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
#ifndef COMPASS_DOCS_CODECS_PATTERN_USES_HPP
#define COMPASS_DOCS_CODECS_PATTERN_USES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's pattern-uses gate: a service architecture pattern
 * and each of its uses link each other.
 *
 * Reads every document under doc/ or projects/ outside build and vcpkg
 * directories that is UTF-8 and has an :ID: line; a later page with an ID
 * already seen replaces the earlier one. A pattern is a page of #+type:
 * service_architecture_pattern. Its "In ORE Studio" section lists its uses
 * and a use's "Patterns used" section lists its patterns, as id links.
 * Pages under doc/plans/, doc/knowledge/external/ and doc/agile/ need not
 * link back. Rules: unknown_use, one_way, not_a_pattern and shared_id.
 */
extern const compass::codec::types::descriptor pattern_uses;

}

#endif

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
#ifndef COMPASS_DOCS_CODECS_RULE_IDENTIFIERS_HPP
#define COMPASS_DOCS_CODECS_RULE_IDENTIFIERS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's rule-identifiers gate (check_rule_identifiers.py):
 * every UX rule carries one identifier and the checklist cites them.
 *
 * The record screen standard (RS) and the entity screen specification (ES)
 * under doc/knowledge/ui/ key their rules by level-two section; a rule is a
 * bullet of such a section outside a block. Rules unkeyed_section,
 * no_identifier, foreign_code and duplicate judge the rules;
 * bad_citation judges each open line of the screen review checklist: its
 * citations have two digits and name a rule a page carries, or the line
 * cites a page with no identifiers yet (UXL, UDP, CFG, FSS, OLS).
 */
extern const compass::codec::types::descriptor rule_identifiers;

}

#endif

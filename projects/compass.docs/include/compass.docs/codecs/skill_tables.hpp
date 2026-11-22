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
#ifndef COMPASS_DOCS_CODECS_SKILL_TABLES_HPP
#define COMPASS_DOCS_CODECS_SKILL_TABLES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief The skill catalogue: the generated region of
 * doc/llm/skills/claude_code_skills.org.
 *
 * Each skill directory under doc/llm/skills with a SKILL.org is grouped by
 * the segment after compass-, in a fixed domain order, as a row of its
 * ID, name, title and description, the description cut to 110 characters.
 * The first region between the BEGIN and END generated catalogue markers,
 * the BEGIN marker naming this codec or the script that wrote the region
 * before it, is replaced under a marker naming this codec; a catalogue
 * without them is not written.
 */
extern const compass::codec::types::descriptor skill_catalogue;

/**
 * @brief The skill type tables: the document types table of
 * compass-doc-add and the read-only commands table of compass-doc-show.
 *
 * The types come from the doc_*.org.mustache templates and the parents
 * compass.py's _STATIC_PARENT and _DEFAULTABLE_PARENT name; the commands
 * from the help of compass.py's subparsers. Every region between a
 * table's markers, its BEGIN marker naming this codec or the script that
 * wrote the table before it, is replaced under a marker naming this codec.
 */
extern const compass::codec::types::descriptor skill_type_tables;

/**
 * @brief The skill levels, from the check document whose Definition names
 * this codec with :CODEC:. Its table "Levels" gives each skill's level and
 * the reason for it; "Systems" the levels in order, each with its System,
 * horizon and attenuation: advisory, seeing its own skills and the cross
 * level's; binding, seeing the skills whose verb "Read-only verbs" lists
 * and nothing else; or cross, whose skills every advisory level sees.
 * "Settings" places the skills and the levels page.
 *
 * Every SKILL.org gets its #+level: line: the first one is replaced, or
 * the line goes after #+export_exclude_tags: noexport. A SKILL.org
 * holding a generated region is that generator's output, so it is not
 * written, and a level it lacks is refused. The levels page
 * gets its generated inventory, the count each System sees, and beside it
 * the generated slices, the skills each level sees; slices a page lacks
 * are put after the inventory. A skill with no row, or a row naming no
 * skill, is a finding and nothing is written; a missing table, column or
 * setting is unreadable.
 */
extern const compass::codec::types::descriptor skill_levels;

}

#endif

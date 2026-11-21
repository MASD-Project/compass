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

#include <span>
#include <string_view>
#include <utility>
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
 * @brief The skill-levels inventory: the generated region of
 * doc/llm/skills/skill_systems_levels.org, the count of skills each
 * System sees by skill_level_table. Every region between the inventory's
 * markers, its BEGIN marker naming this codec or a script that wrote the
 * inventory before it, is replaced under a marker naming this codec. A
 * skill missing from the table, or a table entry with no skill, writes
 * nothing, and the refusal names each; an inventory without its markers
 * is not written.
 */
extern const compass::codec::types::descriptor skill_levels;

/**
 * @brief The table skill_levels applies: each skill and the System it
 * serves, sorted by skill.
 */
[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>>
skill_level_table() noexcept;

}

#endif
